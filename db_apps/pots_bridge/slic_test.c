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
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "cdcs_syslog.h"
#include "./pots_rdb_operations.h"
#include "./fsk/packet.h"
#include "./slic_control/slic_control.h"
#include "./slic_control/tone_pattern.h"

volatile int pots_bridge_running = 1;	// for compile error
static struct slic_t test_slic;

// POTS bridge constants
#define MAX_RDB_EVENTS_PER_SECOND 10
#define MAX_DIGITS 64

typedef struct
{
	char digits[MAX_DIGITS];
	unsigned int size;
	unsigned int begin;
	unsigned int dialed_size;
} number_t;

const char digitmap[] = "D1234567890*#ABC";

const char shortopts[] = "Vhd:s:?";

/* Version Information */
/* 1.2.0 add telephony profile option */
#define VER_MJ		1
#define VER_MN		2
#define VER_BLD		0

void usage()
{
	fprintf(stderr, "\nUsage: slic_test <device> [OPTIONS] <command>\n");
	fprintf(stderr, "<device>\n");
	fprintf(stderr, "\tfor Bovine, don't need\n");
	fprintf(stderr, "\tfor Platypus, '/dev/slic0' or '/dev/slic1'\n");
	fprintf(stderr, "[Options]\n");
	fprintf(stderr, "\t-V: slic_test version information\n");
	fprintf(stderr, "\t-d: device\n");
	/* support international telephony profile */
	fprintf(stderr, "\t-s telephony profile\n");
	fprintf(stderr, "\t   0 : default(AU)\n");
	fprintf(stderr, "\t   1 : Canadian(Telus)\n");
	fprintf(stderr, "\t   2 : North America(Int)\n");
	/* end of support international telephony profile */
	fprintf(stderr, "<command> ::= loop | tones <tones> | fsk <number> | pattern | tone <frequency> <volume> |\n");
	fprintf(stderr, "              dr<distinctive ring id>\n");
	fprintf(stderr, "\tloop: prints loop state (on/off hook)\n");
	fprintf(stderr, "\ttones: play pre-defined tones\n");
	fprintf(stderr, "\t      (e.g: slic_test tones b5d2r3 - busytone 5 sec, dialtone 2 sec, ring 3 sec)\n");
	fprintf(stderr, "\t      (e.g: slic_test tones h5a2c3 - roh 5 sec, amwi 2 sec, recall 3 sec)\n");
	fprintf(stderr, "\tfsk: send FSK-encoded number to the phone\n");
	fprintf(stderr, "\tpattern: play a hard-coded tone pattern\n");
	fprintf(stderr, "\ttone: play a tone and output its settings for SLIC\n");
	fprintf(stderr, "\t      (e.g. slic_test tone 440 50 - play 440Hz tone at 50%% of volume)\n");
	fprintf(stderr, "\tdr: play a pre-defined distinctive ring (dr0, dr1, dr2)\n");
	fprintf(stderr, "<tones> ::= <tone><duration>\n");
	fprintf(stderr, "<tone> ::= b | d | r\n");
	fprintf(stderr, "<duration> ::= 1-9\n");
	fprintf(stderr, "\n");
	exit(2);
}

static const char* slic_tone_names[] = { "none", "dial", "busy", "ring", "roh", "amwi", "recall" };
int slic_play_test(slic_tone_enum tone, unsigned int seconds)
{
	fprintf(stderr, "slic test: testing %s for %d seconds...\n", slic_tone_names[ tone ], seconds);
	slic_play(test_slic.slic_fd, slic_tone_none);
	if (slic_play(test_slic.slic_fd, tone) != 0)
	{
		fprintf(stderr, "slic test: test failed\n");
		return -1;
	}
	sleep(seconds);
	if (slic_play(test_slic.slic_fd, slic_tone_none) != 0)
	{
		fprintf(stderr, "slic test: test failed\n");
		return -1;
	}
	fprintf(stderr, "slic test: test done\n");
	return 0;
}

int slic_play_test_pattern(void)
{
	static const struct tone_t tones[] =
	{
		{ .tone = notes_e4, .volume = 30, .duration = 600 }
		                              , { .tone = notes_rest, .duration = 600 }
		                              , { .tone = notes_e3, .volume = 30, .duration = 1200 }
		                              , { .tone = notes_c3, .volume = 70, .duration = 1200 }
		                              , { .tone = notes_e4, .volume = 40, .duration = 2400 }
		                              , {0,} // terminator of OSC1
		                              , { .tone = notes_c4, .volume = 30, .duration = 600 }
		                              , { .tone = notes_rest, .duration = 600 }
		                              , { .tone = notes_c3, .volume = 30, .duration = 300 }
		                              , { .tone = notes_d3, .volume = 40, .duration = 300 }
		                              , { .tone = notes_e3, .volume = 50, .duration = 300 }
		                              , { .tone = notes_f3, .volume = 60, .duration = 300 }
		                              , { .tone = notes_g3, .volume = 70, .duration = 300 }
		                              , { .tone = notes_f3, .volume = 60, .duration = 300 }
		                              , { .tone = notes_e3, .volume = 50, .duration = 300 }
		                              , { .tone = notes_d3, .volume = 40, .duration = 300 }
		                              , { .tone = notes_c3, .volume = 30, .duration = 2400 }
		                              , {0,} // terminator of OSC2
	                              };
	static struct slic_oscillator_settings_t settings[ sizeof(tones) / sizeof(struct tone_t)];
	unsigned int duration = 0;
	unsigned int size = tones_to_oscillator_settings(settings, tones, &duration);
	struct slic_array_ptr_t p = { .size = size / sizeof(struct slic_oscillator_settings_t), .sizeof_element = sizeof(struct slic_oscillator_settings_t), .data = settings };
	int result = slic_play_pattern(test_slic.slic_fd, &p);
	fprintf(stderr, "sleeping for %d msec...\n", duration);
	usleep(1000 * duration);
	return result;
}

int main(int argc, char **argv)
{
	const char* command;
	const char* c;
	int argi;
	int	ret = 0;
	const char* device = "default";
	/* support international telephony profile */
	tel_profile_type tel_profile = MAX_PROFILE;
	/* end of support international telephony profile */

	while ((ret = getopt(argc, argv, shortopts)) != EOF)
	{
		switch (ret)
		{
			case 'V':
				fprintf(stderr, "%s Version %d.%d.%d\n", argv[0], VER_MJ, VER_MN, VER_BLD);
				break;
			case 'd':
				device = optarg;
				break;
				/* support international telephony profile */
			case 's':
				tel_profile = (tel_profile_type)atoi(optarg);
				break;
				/* end of support international telephony profile */
			case 'h':
			case '?':
				usage();
		}
	}
	ret = 0;
	fprintf(stderr, "slic_test: starting...\n");
	openlog("slic_test", LOG_PID | LOG_PERROR, LOG_LOCAL5);
	setlogmask(LOG_UPTO(5 + LOG_ERR));

	test_slic.slic_fd = test_slic.cid = -1;
	test_slic.call_type = NONE;
	memcpy(&test_slic.dev_name[0], "", sizeof(""));
	test_slic.slic_fd = slic_open(device);
	if (test_slic.slic_fd < 0)
	{
		fprintf(stderr, "slic_test: failed to initialize SLIC [%s]!\n", device);
		exit(1);
	}
	if (slic_get_channel_id(test_slic.slic_fd, &test_slic.cid) < 0)
	{
		fprintf(stderr, "slic_test: failed to get cid\n");
		exit(1);
	}
	strncpy((char *) &test_slic.dev_name[0], (const char *) device, strlen(device));
	/* support international telephony profile */
	if (tel_profile != MAX_PROFILE)
	{
		if (slic_set_tel_profile(test_slic.slic_fd, tel_profile) < 0)
		{
			fprintf(stderr, "failed to set telephony profile\n");
		}
	}
	/* end of support international telephony profile */
	SYSLOG_DEBUG("slic_fd %d, cid %d, dev name %s\n", test_slic.slic_fd, test_slic.cid, test_slic.dev_name);

	argi = optind;
	if (argi == argc)
	{
		usage();
	}
	command = argv[argi];

	if (strcmp(command, "loop") == 0)
	{
		slic_on_off_hook_enum loop_state;
		int result = slic_get_loop_state(test_slic.slic_fd, &loop_state);
		if (result == 0)
		{
			fprintf(stderr, "slic_test: loop state '%s hook'\n", loop_state == slic_on_hook ? "on" : "off");
		}
		else
		{
			fprintf(stderr, "slic_test: failed to get loop state (%s)", strerror(errno));
		}
	}
	else if (strcmp(command, "tones") == 0)
	{
		const char* operations;
		++argi;
		if (argi == argc)
		{
			usage();
		}
		operations = argv[argi];

		for (c = operations; *c; ++c)
		{
			int duration;
			slic_tone_enum tone = slic_tone_none;
			if (*c == 'd') tone = slic_tone_dial;
			else if (*c == 'd') tone = slic_tone_dial;
			else if (*c == 'b') tone = slic_tone_busy;
			else if (*c == 'r') tone = slic_tone_ring;
			else if (*c == 'h') tone = slic_tone_roh;
			else if (*c == 'a') tone = slic_tone_amwi;
			else if (*c == 'c') tone = slic_tone_recall;
			//if( tone == slic_tone_none ) { slic_play_test( slic_tone_none, 1 ); fprintf( stderr, "slic_test: expected command ('b', 'd', or 'r'), got '%c'\n", *c ); usage(); }
			++c;
			if (*c < '1' || *c > '9')
			{
				slic_play_test(slic_tone_none, 1);
				fprintf(stderr, "slic_test: expected duration in seconds, got '%c'\n", *c);
				usage();
			}
			duration = *c - '0';
			ret = slic_play_test(tone, duration);
		}
	}
	else if (strcmp(command, "fsk") == 0)
	{
		const char* clip;
		++argi;
		if (argi == argc)
		{
			usage();
		}
		clip = argv[argi];
		fsk_packet packet;
		fsk_packet_clip(&packet, clip);
		slic_fsk_send(test_slic.slic_fd, &packet);
		//slic_play(test_slic.slic_fd, slic_tone_ring);
		fprintf(stderr, "slic_test: waiting for %d s\n", (2+strlen(clip)/5));
		sleep(2+strlen(clip)/5);
	}
	else if (strcmp(command, "pattern") == 0)
	{
		fprintf(stderr, "slic_test: testing fixed pattern...\n");
		ret = slic_play_test_pattern();
	}
	else if (strcmp(command, "tone") == 0)
	{
		struct tone_t tones[] = { {0,}, {0,}, {0,}, {0,} };
		struct slic_oscillator_settings_t settings[ sizeof(tones) / sizeof(struct tone_t)];
		unsigned int duration;
		struct slic_array_ptr_t p;
		unsigned int idx = argi;

		if (argv[idx+1] != 0)
		{
			tones[0].tone = atoi(argv[idx+1]);
			if (tones[0].tone == 0) usage();
		}
		else usage();
		if (argv[idx+2] != 0)
		{
			tones[0].volume = atoi(argv[idx+2]);
			if (tones[0].volume == 0) usage();
			tones[0].duration = 5000;
		}
		else usage();
		if (argv[idx+3] != 0)
		{
			tones[2].tone = atoi(argv[idx+3]);
			if (tones[2].tone == 0) usage();
			if (argv[idx+4] != 0)
			{
				tones[2].volume = atoi(argv[idx+4]);
				if (tones[2].volume == 0) usage();
				tones[2].duration = 5000;
			}
			else usage();
		}
		p.data = settings;
		p.sizeof_element = sizeof(struct slic_oscillator_settings_t);
		p.size = tones_to_oscillator_settings(settings, tones, &duration) / p.sizeof_element;
		slic_play_pattern(test_slic.slic_fd, &p);
		usleep(1000 * duration);
		fprintf(stderr, "%d Hz -> 0x%04X; volume of %d %% -> 0x%04X\n", tones[0].tone, settings[0].frequency, tones[0].volume, settings[0].amplitude);
		if (tones[2].tone != 0)
		{
			fprintf(stderr, "%d Hz -> 0x%04X; volume of %d %% -> 0x%04X\n", tones[2].tone, settings[2].frequency, tones[2].volume, settings[2].amplitude);
		}
	}
	else if (memcmp(command, "dr", 2) == 0)
	{
		int i = atoi(command + 2);
		if (i < slic_distinctive_ring_0 || i > slic_distinctive_ring_canada)
		{
			usage();
		}
		else
		{
			/* append slic_led_control */
			slic_handle_pots_led(test_slic.cid, led_flash_on);
			slic_play_distinctive_ring(test_slic.slic_fd, (slic_distinctive_ring_enum)i);
			sleep(10);
			/* append slic_led_control */
			slic_handle_pots_led(test_slic.cid, led_flash_off);
		}
	}
	else
	{
		usage();
	}

	slic_close(test_slic.slic_fd);
	closelog();
	fprintf(stderr, "slic_test: %s!\n", (ret ? "failed" : "done"));
	exit(ret);
}

/*
* vim:ts=4:sw=4:
*/

