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
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef USE_ALSA
#include "slic/slic_ioctl.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "cdcs_syslog.h"
#include "./slic_control.h"
#include "./tone_pattern.h"
#include "../pots_rdb_operations.h"

pots_call_type db_slic_call_type[MAX_CHANNEL_NO];

const char* call_type_name[MAX_CALL_TYPE_INDEX] = { "none", "3GVoice", "VOIP", "FOIP" };

struct slic_t slic_info[MAX_CHANNEL_NO];

static const struct tone_t tone_enabled[] =
{
									{ .tone = notes_f4, .volume = 70, .duration = 600 }
								, { .tone = notes_rest, .volume = 0, .duration = 100 }
								, { .tone = notes_f4, .volume = 70, .duration = 600 }
								, { .tone = notes_rest, .volume = 0, .duration = 100 }
								, {0,} // terminator of OSC1
								//, { .tone = notes_g3, .volume = 70, .duration = 100 }
								//, { .tone = notes_rest, .volume = 0, .duration = 50 }
								//, { .tone = notes_g4, .volume = 70, .duration = 600 }
								, {0,} // terminator of OSC2
							};

static const struct tone_t tone_disabled[] =
{
									{ .tone = notes_b4, .volume = 70, .duration = 600 }
								, { .tone = notes_rest, .volume = 0, .duration = 100 }
								, { .tone = notes_b4, .volume = 70, .duration = 600 }
								, { .tone = notes_rest, .volume = 0, .duration = 100 }
								, {0,} // terminator of OSC1
								//, { .tone = notes_g4, .volume = 70, .duration = 100 }
								//, { .tone = notes_rest, .volume = 0, .duration = 50 }
								//, { .tone = notes_g3, .volume = 70, .duration = 600 }
								, {0,} // terminator of OSC2
							};

static const struct tone_t tone_failure[] =
{
									{ .tone = notes_c4, .volume = 70, .duration = 400 }
								, { .tone = notes_f3, .volume = 70, .duration = 400 }
								, {0,} // terminator of OSC1
								, {0,} // terminator of OSC2
							};

struct tone_pattern_t_
{
	const char* name;
	unsigned int duration;
	unsigned int size;
	const struct tone_t* pattern;
	struct slic_oscillator_settings_t oscillator_settings[16];
};

static struct tone_pattern_t_ tone_patterns[] = {   { .name = "failure", .pattern = tone_failure }
			, { .name = "enabled", .pattern = tone_enabled }
			, { .name = "disabled", .pattern = tone_disabled }
		};

static int init_tone_patterns(void)
{
	unsigned int i;
	unsigned int size = sizeof(tone_patterns) / sizeof(struct tone_pattern_t_);
	for (i = 0; i < size; ++i)
	{
		tone_patterns[i].size = tones_to_oscillator_settings(tone_patterns[i].oscillator_settings, tone_patterns[i].pattern, &tone_patterns[i].duration);
	}
	return 0;
}

int slic_open(const char* device)
{
#ifdef USE_ALSA
	const char* d = *device ? device : "default";
	if (!alsa_open(d))
	{
		SYSLOG_ERR("failed to open sound control device '%s', '%s'!", device, d);
		return -1;
	}
	if (!alsa_control_handle())
	{
		SYSLOG_ERR("alsa not open yet!");
		return 1;
	}
	init_tone_patterns();
	return 0;
#else
	int slic_fd;
	slic_fd = open(device, O_RDWR);
	if (slic_fd < 0)
	{
		SYSLOG_ERR("failed to open '%s' (%s)!", device, strerror(errno));
		return slic_fd;
	}
	init_tone_patterns();
	return slic_fd;
#endif
}

void slic_close(int slic_fd)
{
	slic_register_event_handler(slic_event_dtmf, NULL);
	slic_register_event_handler(slic_event_dtmf_complete, NULL);
	slic_register_event_handler(slic_event_loop_state, NULL);
	slic_register_event_handler(slic_event_fsk_complete, NULL);
	slic_register_event_handler( slic_event_vmwi_complete, NULL );

	slic_play(slic_fd, slic_tone_none);

#ifdef USE_ALSA
	alsa_close();
#else
	if (slic_fd < 0)
	{
		return;
	}
	close(slic_fd);
#endif
}

int slic_play(int slic_fd, slic_tone_enum tone)
{
#ifdef USE_ALSA
	return alsa_set_snd_card_ctl(SLIC_TONE_CONTROL_NAME, tone, 0);
#else
	return ioctl(slic_fd, slic_ioctl_play_tone, tone);
#endif
}

int slic_play_pattern(int slic_fd, const struct slic_array_ptr_t* pattern)
{
#ifdef USE_ALSA
	alsa_set_snd_card_ctl_bytes(SLIC_TONE_PATTERN_NAME, pattern->data, pattern->size * pattern->sizeof_element);
	return 0;
#else
	return ioctl(slic_fd, slic_ioctl_play_pattern, pattern);
#endif
}

int slic_play_fixed_pattern(int slic_fd, slic_tone_pattern_t pattern, int is_blocking)
{
	int result = 0;
	struct slic_array_ptr_t p = { .data = tone_patterns[ pattern ].oscillator_settings, .size = tone_patterns[ pattern ].size / sizeof(struct slic_oscillator_settings_t), .sizeof_element = sizeof(struct slic_oscillator_settings_t) };
	SYSLOG_DEBUG("pattern: '%s'; %s (duration = %d msec)", tone_patterns[ pattern ].name, is_blocking ? "blocking" : "non-blocking", tone_patterns[ pattern ].duration);
	if ((result = slic_play_pattern(slic_fd, &p)) < 0)
	{
		return result;
	}
	if (is_blocking)
	{
		usleep(tone_patterns[ pattern ].duration * 1000);
	}
	return 0;
}

int slic_fsk_send(int slic_fd, const fsk_packet* packet)
{
#ifdef USE_ALSA
	return alsa_set_snd_card_ctl_bytes(SLIC_FSK_CONTROL_NAME, (void*)packet, sizeof(fsk_packet));
#else
	return ioctl(slic_fd, slic_ioctl_send_fsk, packet);
#endif
}

/* support international telephony profile */
static const char* distinctive_ring_names[] = { "DR0", "DR1", "DR2" };
/* end of support international telephony profile */
static unsigned int distinctive_ring_0_pattern[] = { 400, 200, 400, 2000 };
static unsigned int distinctive_ring_1_pattern[] = { 400, 400, 200, 200, 400, 1400 };
/* support international telephony profile */
static unsigned int distinctive_ring_2_pattern[] = { 2000, 4000 };
/* end of support international telephony profile */
static struct slic_array_ptr_t distinctive_rings[] =
{
	{ .size = sizeof(distinctive_ring_0_pattern) / sizeof(unsigned int), .sizeof_element = sizeof(unsigned int), .data = distinctive_ring_0_pattern, }
			, { .size = sizeof(distinctive_ring_1_pattern) / sizeof(unsigned int), .sizeof_element = sizeof(unsigned int), .data = distinctive_ring_1_pattern, }
			/* support international telephony profile */
			, { .size = sizeof(distinctive_ring_2_pattern) / sizeof(unsigned int), .sizeof_element = sizeof(unsigned int), .data = distinctive_ring_2_pattern, }
			/* end of support international telephony profile */
		};

int slic_play_distinctive_ring(int slic_fd, slic_distinctive_ring_enum pattern)
{
	SYSLOG_DEBUG("distinctive ring '%s'", distinctive_ring_names[ pattern ]);

#ifdef USE_ALSA
	alsa_set_snd_card_ctl_bytes(SLIC_DISTINCTIVE_RING_NAME, distinctive_rings[ pattern ].data, distinctive_rings[ pattern ].size * distinctive_rings[ pattern ].sizeof_element);
	return 0;
#else
	return ioctl(slic_fd, slic_ioctl_distinctive_ring, &distinctive_rings[ pattern ]);
#endif
}

int slic_get_loop_state(int slic_fd, slic_on_off_hook_enum* loop_state)
{
#ifdef USE_ALSA
	int i;
	if (alsa_get_snd_card_ctl(SLIC_LOOP_STAT_NAME, &i) == 0)
	{
		*loop_state = i;
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_get_loop_state, loop_state) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to get loop state (%s)!", strerror(errno));
	return -1;
}

int slic_enable_pcm_dynamic(int slic_fd, BOOL enabled, int idx)
{
	static char model[1024] = {0, };
	slic_pcm_type pcm_type = slic_pcm_type_off;

#ifdef HAS_VOIP_FEATURE
	/* special process for VOIP call. use linear mode */
	if (slic_info[idx].call_type == VOICE_ON_IP)
	{
		if (enabled)
		{
				pcm_type = slic_pcm_type_lin;
		}
		return slic_enable_pcm(slic_fd, pcm_type);
	}
#endif	/* HAS_VOIP_FEATURE */

	if (!strlen(model))
	{
		if (rdb_get_single(RDB_MODEM_NAME, model, sizeof(model)) < 0)
			model[0] = 0;
	}

	if (enabled)
	{
		//if(!strcmp(model,"MC8790V"))
//		if (!strncmp(model, "MC", 2))		/* for all Sierra modem */
//			pcm_type = slic_pcm_type_even_alaw;
//		else
			pcm_type = slic_pcm_type_lin;
	}

	SYSLOG_DEBUG("model_name=%s, pcm_type = 0x%04x", model, pcm_type);

	return slic_enable_pcm(slic_fd, pcm_type);
}

int slic_enable_pcm(int slic_fd, slic_pcm_type pcm_type)
{
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl(SLIC_MODE_CONTROL_NAME, 0xff, pcm_type) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_enable_pcm, pcm_type) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to %sable PCM (%s)!", pcm_type ? "en" : "dis", strerror(errno));
	return -1;
}

int slic_set_loopback_mode(int slic_fd, int loopback_mode)
{
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl(SLIC_LOOPBACK_MODE_NAME, loopback_mode, 0) == 0)
	{
		return 0;
	}
#else
	SYSLOG_DEBUG("loopback test mode %d", loopback_mode);
	if (ioctl(slic_fd, slic_ioctl_loopback, loopback_mode) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to set loopback mode (%s)!", strerror(errno));
	return -1;
}

int slic_set_multi_channel_loopback_mode(int slic_fd, int on_off)
{
#ifdef USE_ALSA
	return 0;
#else
	SYSLOG_DEBUG("multi channel loopback test mode %d", on_off);
	if (ioctl(slic_fd, slic_ioctl_multi_channel_loopback, on_off) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to set multi channel loopback mode (%s)!", strerror(errno));
	return -1;
}

/* change PCM clk selecting gpio to phone module when phone module was detected. */
/* direction 1 : phone module, 0 : processor */
int slic_change_pcm_clk_source(int slic_fd, int direction)
{
	SYSLOG_DEBUG("slic PCM clock source change to %s", direction ? "phone module" : "processor");
#ifdef USE_ALSA
	/* share pcm control for pcm clock source change, default 0 (processor) --> 1 (modem) */
	if (alsa_set_snd_card_ctl(SLIC_MODE_CONTROL_NAME, 1, 0) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_pcm_clock_change, direction) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("*****************************************************************");
	SYSLOG_ERR("Critical Error : failed to change PCM clock source (%s)!", strerror(errno));
	SYSLOG_ERR("*****************************************************************");
	return -1;
}

/* append slic_led_control */
int slic_get_channel_id(int slic_fd, int* cid)
{
#ifdef USE_ALSA
	*cid = 0;
	return 0;
#else
	if (ioctl(slic_fd, slic_ioctl_get_cid, cid) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to get channel ID (%s)!", strerror(errno));
	return -1;
}

typedef int (*slic_callback_t)(int cid, const struct slic_event_t*);
static slic_callback_t loop_state_callback = NULL;
static slic_callback_t dtmf_callback = NULL;
static slic_callback_t dtmf_complete_callback = NULL;
static slic_callback_t fsk_complete_callback = NULL;
static slic_callback_t vmwi_complete_callback = NULL;

#ifdef USE_ALSA
static int handle_slic_event_dtmf(snd_hctl_elem_t *elem, unsigned int mask)
{
	int	err, value;
	struct slic_event_t event;
	if (dtmf_callback == NULL)
	{
		return 0;
	}
	if ((err = alsa_get_elem_value(elem, &value)) != 0)
	{
		return err;
	}
	event.data.dtmf_digit = value & 0x0F;
	event.type = slic_event_dtmf;
	return dtmf_callback(0, &event);
}

static int handle_slic_event_dtmf_complete(snd_hctl_elem_t *elem, unsigned int mask)
{
	/* TODO : not implemented yet */
	return 0;
}

static int handle_slic_event_loop_state(snd_hctl_elem_t *elem, unsigned int mask)
{
	int	err, value;
	struct slic_event_t event;
	if (loop_state_callback == NULL)
	{
		return 0;
	}
	if ((err = alsa_get_elem_value(elem, &value)) != 0)
	{
		return err;
	}
	event.data.loop_state = (slic_on_off_hook_enum)value;
	event.type = slic_event_loop_state;
	return loop_state_callback(0, &event);
}
static int handle_slic_event_fsk_complete(snd_hctl_elem_t *elem, unsigned int mask)
{
	struct slic_event_t event;
	if (fsk_complete_callback == NULL)
	{
		return 0;
	}
	event.type = slic_event_fsk_complete;
	return fsk_complete_callback(0, &event);
}
static int handle_slic_event_vmwi_complete( snd_hctl_elem_t *elem, unsigned int mask )
{
	struct slic_event_t event;
	if (vmwi_complete_callback == NULL)
	{
		return 0;
	}
	event.type = slic_event_vmwi_complete;
	return vmwi_complete_callback(0, &event);
}
#endif

int slic_register_event_handler(slic_event_enum event, int (*callback)(int idx, const struct slic_event_t*))
{
	switch (event)
	{
		case slic_event_loop_state:
			loop_state_callback = callback;
#ifdef USE_ALSA
			return alsa_register_snd_card_ctl_callback(SLIC_LOOP_STAT_NAME, callback ? handle_slic_event_loop_state : NULL, NULL);
#else
			return 0;
#endif
		case slic_event_dtmf:
			dtmf_callback = callback;
#ifdef USE_ALSA
			return alsa_register_snd_card_ctl_callback(SLIC_DTMF_DIGIT_NAME, callback ? handle_slic_event_dtmf : NULL, NULL);
#else
			return 0;
#endif
		case slic_event_dtmf_complete:
			dtmf_complete_callback = callback;
#ifdef USE_ALSA
			/* TODO : not implemented yet */
			return 0;
#else
			return 0;
#endif
		case slic_event_fsk_complete:
			fsk_complete_callback = callback;
#ifdef USE_ALSA
			return alsa_register_snd_card_ctl_callback(SLIC_FSK_COMPLETE_NAME, callback ? handle_slic_event_fsk_complete : NULL, NULL);
#else
			return 0;
#endif
		case slic_event_vmwi_complete:
			vmwi_complete_callback = callback;
#ifdef USE_ALSA
			return alsa_register_snd_card_ctl_callback(SLIC_VMWI_COMPLETE_NAME, callback ? handle_slic_event_vmwi_complete : NULL, NULL);
#else
			return 0;
#endif
	}
	SYSLOG_ERR("don't know how to handle event %d", event);
	return -1;
}

void slic_handle_events(int idx)
{
#ifdef USE_ALSA
	alsa_handle_events();
#else
	struct slic_event_t event;
	if (ioctl(slic_info[idx].slic_fd, slic_ioctl_get_event, &event) < 0)
	{
		SYSLOG_ERR("ioctl() failed (%s)", strerror(errno));
		return;
	}
	switch (event.type)
	{
		case slic_event_loop_state:
			loop_state_callback ? loop_state_callback(idx, &event) : 0;
			return;
		case slic_event_dtmf:
			dtmf_callback ? dtmf_callback(idx, &event) : 0;
			return;
		case slic_event_dtmf_complete:
			dtmf_complete_callback ? dtmf_complete_callback(idx, &event) : 0;
			return;
		case slic_event_fsk_complete:
			fsk_complete_callback ? fsk_complete_callback(idx, &event) : 0;
			return;
		case slic_event_vmwi_complete:
			vmwi_complete_callback ? vmwi_complete_callback( idx, &event ) : 0;
			return;
	}
	SYSLOG_ERR("don't know how to handle event type %d", event.type);
#endif
}

/* append slic_led_control */
void slic_handle_pots_led(int cid, pots_led_control_type mode)
{
	char cmdline[128] = "";
	int block_slic_ledcontrol=0;
	char led_test_mode[64];

#if ( defined(BOARD_3g38wv) || defined(BOARD_3g38wv) || defined(BOARD_3g38wv2) ) && defined(SKIN_ro)
	char achSimStat[64];

	if (rdb_get_single("simledctl.command.trigger", achSimStat, sizeof(achSimStat)) == 0) {
		if(!strcmp(achSimStat,  "pinlock") || !strcmp(achSimStat,  "puklock") || !strcmp(achSimStat,  "meplock")) {
			block_slic_ledcontrol = 1;
		}
	}
	
#endif
	// override led for factory mode
	if (rdb_get_single("factory.led_test_mode", led_test_mode, sizeof(led_test_mode)) == 0) {
		if(atoi(led_test_mode))
			block_slic_ledcontrol = 1;
	}
	
	if (block_slic_ledcontrol == 0) {
		sprintf(cmdline, "led sys pots%d state %d", cid, mode);
		(void)system(cmdline);
	}
}


void initialize_slic_variables(void)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO; i++)
	{
		slic_info[i].slic_fd = slic_info[i].cid = -1;
		slic_info[i].call_active = FALSE;
		memcpy((void *) &slic_info[i].dev_name[0], (const void *)"", sizeof(""));
		slic_info[i].call_type = NONE;
	}
}

void assign_slic_channel_to_call_fuction(int index, struct slic_t *slic_p)
{
#ifdef HAS_VOIP_FEATURE
	slic_p->call_type = db_slic_call_type[index];
#else
	slic_p->call_type = VOICE_ON_3G;
#endif
	SYSLOG_DEBUG("slic idx %d : ch [%d] was assigned for %s\n", index, slic_p->cid, call_type_name[slic_p->call_type]);
}

void assign_slic_channel_to_rdb_variables(int index, struct slic_t *slic_p)
{
#ifdef HAS_VOIP_FEATURE
	static int voip_idx = 0;
	if (slic_p->call_type == VOICE_ON_IP)
	{
		sprintf(slic_p->rdb_name_at_cmd, "%s.%d", RDB_VOIP_CMD_PREFIX, voip_idx);
		sprintf(slic_p->rdb_name_phone_cmd, "%s", "");
		sprintf(slic_p->rdb_name_calls_list, "%s.%d.%s", RDB_VOIP_CALLS_PREFIX, voip_idx, RDB_CALLS_LIST);
		sprintf(slic_p->rdb_name_cmd_result, "%s.%d.%s", RDB_VOIP_CMD_PREFIX, voip_idx, RDB_CMD_RESULT);
		sprintf(slic_p->rdb_name_dtmf_cmd, "%s.%d", RDB_UMTS_DTMF_PREFIX, voip_idx);
		voip_idx++;
	}
	else
	{
		sprintf(slic_p->rdb_name_at_cmd, "%s", RDB_UMTS_CMD_PREFIX);
		sprintf(slic_p->rdb_name_phone_cmd, "%s", RDB_PHONE_CMD_PREFIX);
		sprintf(slic_p->rdb_name_calls_list, "%s.%s", RDB_UMTS_CALLS_PREFIX, RDB_CALLS_LIST);
		sprintf(slic_p->rdb_name_cmd_result, "%s.%s", RDB_UMTS_CMD_PREFIX, RDB_CMD_RESULT);
		sprintf(slic_p->rdb_name_dtmf_cmd, "%s", RDB_UMTS_DTMF_PREFIX);
	}
#else
	sprintf(slic_p->rdb_name_at_cmd, "%s", RDB_UMTS_CMD_PREFIX);
	sprintf(slic_p->rdb_name_phone_cmd, "%s", RDB_PHONE_CMD_PREFIX);
	sprintf(slic_p->rdb_name_calls_list, "%s.%s", RDB_UMTS_CALLS_PREFIX, RDB_CALLS_LIST);
	sprintf(slic_p->rdb_name_cmd_result, "%s.%s", RDB_UMTS_CMD_PREFIX, RDB_CMD_RESULT);
	sprintf(slic_p->rdb_name_dtmf_cmd, "%s", RDB_UMTS_DTMF_PREFIX);
#endif
}

void display_slic_info_db(void)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO ; i++)
	{
		SYSLOG_DEBUG("idx %d : slic_fd %d, cid %d, dev name %s, call type %s\n",
					i, slic_info[i].slic_fd, slic_info[i].cid, slic_info[i].dev_name, call_type_name[slic_info[i].call_type]);
	}
}

int get_cid_from_slic_fd(int slic_fd)
{
	int i, cid = 0;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (slic_info[i].slic_fd == slic_fd)
		{
			cid = slic_info[i].cid;
			break;
		}
	}
	return cid;
}

int get_max_slic_fd(void)
{
	int i, max_fd = 0;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++) if (slic_info[i].slic_fd >= max_fd) max_fd = slic_info[i].slic_fd;
	return max_fd;
}

int check_call_resource_available(int index, pots_call_type call_type)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		if (i == index) continue;
		if (slic_info[i].call_type == call_type)
		{
			/* Multiple call is enabled for VOIP call type */
			if (slic_info[i].call_active == TRUE && call_type == VOICE_ON_3G)
			{
				SYSLOG_DEBUG("[ %s ] was assigned already idx %d, cid %d, slic_fd %d\n",
						call_type_name[slic_info[i].call_type], i, slic_info[i].cid, slic_info[i].slic_fd);
				return 0;
			}
		}
	}
	return 1;
}

void set_slic_call_active_state(int index, BOOL active)
{
	if (slic_info[index].call_active != active)
		SYSLOG_DEBUG("idx [%d] call active state changed %d --> %d\n", index, slic_info[index].call_active, active);
	slic_info[index].call_active = active;
}

int get_slic_first_call_active_index(pots_call_type call_type)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		//SYSLOG_DEBUG("slic_info[%d].call_type %d, slic_info[%d].call_active %d", i,slic_info[i].call_type,i, slic_info[i].call_active  );
		if (slic_info[i].call_type == call_type && slic_info[i].call_active == TRUE)
		{
			return i;
		}
	}
	return -1;
}

int get_slic_next_call_active_index(pots_call_type call_type, int idx)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		//SYSLOG_DEBUG("slic_info[%d].call_type %d, slic_info[%d].call_active %d", i,slic_info[i].call_type,i, slic_info[i].call_active  );
		if (i == idx)
		{
			continue;
		}
		if (slic_info[i].call_type == call_type && slic_info[i].call_active == TRUE)
		{
			return i;
		}
	}
	return -1;
}

int get_slic_other_call_active_index(int idx)
{
	int i;
	for (i = 0; i < MAX_CHANNEL_NO && slic_info[i].slic_fd >= 0; i++)
	{
		//SYSLOG_DEBUG("slic_info[%d].call_type %d, slic_info[%d].call_active %d", i,slic_info[i].call_type,i, slic_info[i].call_active  );
		if (i == idx)
		{
			continue;
		}
		if (slic_info[i].call_active == TRUE)
		{
			return i;
		}
	}
	return -1;
}

int is_match_slic_call_type(int index, pots_call_type call_type)
{
	if (slic_info[index].call_type == call_type)
	{
		return TRUE;
	}
	return FALSE;
}

/* slic cal data save/restore feature */
int slic_initialize_device_hw(int slic_fd, slic_cal_data_type *cal_data_p, BOOL need_cal)
{
#ifdef USE_ALSA
	return 0;
#else
	if (need_cal)
	{
		SYSLOG_INFO("SLIC Cal data exist, skip calibration & using stored cal data");
	}
	else
	{
		SYSLOG_INFO("SLIC Cal data no exist, need to calibrate");
	}
	if (ioctl(slic_fd, slic_ioctl_driver_init, cal_data_p) == 0)
	{
		return 0;
	}
	SYSLOG_ERR("failed to initialize SLIC HW device driver (%s)!", strerror(errno));
	return -1;
#endif
}
/* end of slic cal data save/restore feature */

/* support international telephony profile */
int slic_set_tel_profile(int slic_fd, tel_profile_type tel_profile)
{
	SYSLOG_INFO("set telephony profile to %d", tel_profile);
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl(SLIC_TEL_PROFILE_NAME, tel_profile, 0) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_tel_profile, tel_profile) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to set telephony profile (%s)!", strerror(errno));
	return -1;
}
/* end of support international telephony profile */

/* slic register content display */
int slic_display_reg(int slic_fd)
{
	//SYSLOG_INFO("slic register display : slic fd %d", slic_fd);
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl(SLIC_REG_DISPLAY_NAME, 0, 0) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_display_reg) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to display regs (%s)!", strerror(errno));
	return -1;
}
/* end of slic register content display */

/* free run mode set */
int slic_set_freerun_mode(int slic_fd, int freerun_mode)
{
	SYSLOG_INFO("freerun mode set : slic fd %d to %d", slic_fd, freerun_mode);
#ifdef USE_ALSA
	/* Si3210 does not support this feature */
	return 0;
#else
	if (ioctl(slic_fd, slic_ioctl_freerun_mode, freerun_mode) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to set freerun mode (%s)!", strerror(errno));
	return -1;
}
/* end of free run mode set */


/* Bell Canada VMWI feature */
int slic_send_vmwi_fsk( int slic_fd, const fsk_packet* packet )
{
#ifdef USE_ALSA
	return alsa_set_snd_card_ctl_bytes(SLIC_VMWI_CONTROL_NAME, (void*)packet, sizeof(fsk_packet));
#else
	return ioctl( slic_fd, slic_ioctl_send_vmwi, packet );
#endif
}
/* end of Bell Canada VMWI feature */

int slic_tx_mute_override(int slic_fd, int mute_override)
{
	SYSLOG_INFO("slic Tx mute override set : slic fd %d, override %d", slic_fd, mute_override);
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl(SLIC_TXMUTE_OVERRIDE_NAME, mute_override, 0) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_tx_mute_override, mute_override) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to override slic tx mute (%s)!", strerror(errno));
	return -1;
}

int slic_check_mode(int slic_fd, char *mode_name)
{
	//SYSLOG_INFO("slic_check_mode : slic fd %d", slic_fd);
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl(SLIC_CHECK_MODE_NAME, 0, 0) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_check_mode, mode_name) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to check slic mode (%s)!", strerror(errno));
	return -1;
}

int slic_toggle_debug_mode(int slic_fd, int debug_mode)
{
	SYSLOG_INFO("slic_toggle_debug_mode : debug_mode %d", debug_mode);
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl(SLIC_TOGGLE_DEBUG_MODE_NAME, debug_mode, 0) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_debug_mode, debug_mode) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to change slic debug mode (%s)!", strerror(errno));
	return -1;
}

int slic_reg_rw(int slic_fd, reg_rw_cmd_type *cmd)
{
#ifdef USE_ALSA
	if (alsa_set_snd_card_ctl_bytes(SLIC_REG_RW_NAME, (void*)cmd, sizeof(reg_rw_cmd_type)) == 0)
	{
		return 0;
	}
#else
	if (ioctl(slic_fd, slic_ioctl_reg_rw, cmd) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to rw slic reg. (%s)!", strerror(errno));
	return -1;
}

int slic_set_dtmf_tx_mode(int slic_fd, dtmf_tx_type tx_mode)
{
	SYSLOG_INFO("slic_set_dtmf_tx_mode : slic fd %d, mode %d", slic_fd, tx_mode);
#ifdef USE_ALSA
	/* Si3210 not implemented this feature yet */
	return 0;
#else
	if (ioctl(slic_fd, slic_ioctl_set_dtmf_tx_mode, tx_mode) == 0)
	{
		return 0;
	}
#endif
	SYSLOG_ERR("failed to set DTMF tx mode (%s)!", strerror(errno));
	return -1;
}

int get_cmd_retry_count(int index)
{
	return (slic_info[index].call_type == VOICE_ON_IP? 1:0);
}

const char cpld_exec_file[30] = "/usr/sbin/cpld";
const char sys_exec_file[30] = "/usr/sbin/sys";
const char *cpld_mode_name[] = { "loop", "voice", "buf" };
void slic_change_cpld_mode(slic_cpld_mode cpld_mode)
{
	struct stat st;
	char command_line[30] = {0, };

	if(stat(cpld_exec_file, &st) != 0)
	{
		if(stat(sys_exec_file, &st) != 0)
		{
			SYSLOG_ERR("CRITICAL ERROR : can not find 'cpld' nor 'sys' file !!");
			return;
		}
		sprintf(command_line, "%s -c %s", sys_exec_file, cpld_mode_name[cpld_mode]);
	}
	else
	{
		sprintf(command_line, "%s %s", cpld_exec_file, cpld_mode_name[cpld_mode]);
	}
	SYSLOG_INFO("system call '%s'", command_line);
	system(command_line);
}

#define DTMF_TEST_VOLUME			50
#define DTMF_TEST_DURATION			200
#define DTMF_TEST_INTERVAL			100
#define DTMF_TEST_DURATION_SHORT	50
#define DTMF_TEST_INTERVAL_SHORT	25
#define DTMF_HIGH_FREQENCY_1209		1209
#define DTMF_HIGH_FREQENCY_1336		1336
#define DTMF_HIGH_FREQENCY_1477		1477
#define DTMF_HIGH_FREQENCY_1633		1633
#define DTMF_LOW_FREQENCY_697		697
#define DTMF_LOW_FREQENCY_770		770
#define DTMF_LOW_FREQENCY_852		852
#define DTMF_LOW_FREQENCY_941		941
static const struct tone_t dtmf_table[16][2] =
{
	/*                   high frequency                             low frequency             */
	{ { DTMF_HIGH_FREQENCY_1336, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_941, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 0 */
	{ { DTMF_HIGH_FREQENCY_1209, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_697, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 1 */
	{ { DTMF_HIGH_FREQENCY_1336, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_697, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 2 */
	{ { DTMF_HIGH_FREQENCY_1477, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_697, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 3 */
	{ { DTMF_HIGH_FREQENCY_1209, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_770, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 4 */
	{ { DTMF_HIGH_FREQENCY_1336, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_770, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 5 */
	{ { DTMF_HIGH_FREQENCY_1477, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_770, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 6 */
	{ { DTMF_HIGH_FREQENCY_1209, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_852, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 7 */
	{ { DTMF_HIGH_FREQENCY_1336, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_852, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 8 */
	{ { DTMF_HIGH_FREQENCY_1477, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_852, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF 9 */
	{ { DTMF_HIGH_FREQENCY_1209, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_941, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF * */
	{ { DTMF_HIGH_FREQENCY_1477, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_941, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF # */
	{ { DTMF_HIGH_FREQENCY_1633, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_697, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF A */
	{ { DTMF_HIGH_FREQENCY_1633, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_770, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF B */
	{ { DTMF_HIGH_FREQENCY_1633, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_852, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF C */
	{ { DTMF_HIGH_FREQENCY_1633, DTMF_TEST_VOLUME, DTMF_TEST_DURATION }, { DTMF_LOW_FREQENCY_941, DTMF_TEST_VOLUME, DTMF_TEST_DURATION } },		/* DTMF D */
};
int slic_play_dtmf_tones(int slic_fd, const char* dial, BOOL autodial_mode)
{
	struct tone_t tones[] = { {0,}, {0,}, {0,}, {0,} };
	struct slic_oscillator_settings_t settings[ sizeof(tones) / sizeof(struct tone_t)];
	unsigned int duration;
	struct slic_array_ptr_t p;
	unsigned int idx = 0;
	char* c;
	int	ret = 0;

	for (c = (char *) & dial[0]; *c; c++)
	{
		if (!((*c >= '0' && *c <= '9') || (*c == '*') || (*c == '#')  || (*c >= 'A' && *c <= 'D') || (*c >= 'a' && *c <= 'd'))) continue;
		if (*c >= '0' && *c <= '9')	idx = *c - '0';
		else if (*c == '*') idx  = 10;
		else if (*c == '#') idx  = 11;
		else if (*c >= 'A' && *c <= 'D')	idx = *c - 'A';
		else if (*c >= 'a' && *c <= 'd')	idx = *c - 'a';
		tones[0] = dtmf_table[idx][0];
		tones[2] = dtmf_table[idx][1];
		if (autodial_mode)
		{
			tones[0].duration = DTMF_TEST_DURATION_SHORT;
			tones[2].duration = DTMF_TEST_DURATION_SHORT;
		}
		p.data = settings;
		p.sizeof_element = sizeof(struct slic_oscillator_settings_t);
		p.size = tones_to_oscillator_settings(settings, tones, &duration) / p.sizeof_element;
		ret = slic_play_pattern(slic_fd, &p);
		if (autodial_mode)
			usleep(1000 * DTMF_TEST_DURATION_SHORT);
		else
			usleep(1000 * DTMF_TEST_DURATION);
		slic_play(slic_fd, slic_tone_none);
		if (autodial_mode)
			usleep(1000 * DTMF_TEST_INTERVAL_SHORT);
		else
			usleep(1000 * DTMF_TEST_INTERVAL);
	}
	return ret;
}
