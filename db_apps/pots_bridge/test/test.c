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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <getopt.h>
#include <unistd.h>
#include "unit_test.h"
#include "../calls/calls.h"
#include "../pots_rdb_operations.h"
#include "../slic_control/tone_pattern.h"

#include <stdarg.h>

int pots_bridge_running = 1;	// for compile error

void TEST_AUTO(test_tone_pattern)
{
	static const struct tone_t tones[] =
	{
		{ .tone = notes_c3, .volume = 40, .duration = 300 }
		                              , { .tone = notes_d3, .volume = 45, .duration = 300 }
		                              , { .tone = notes_e3, .volume = 50, .duration = 300 }
		                              , { .tone = notes_f3, .volume = 55, .duration = 300 }
		                              , { .tone = notes_g3, .volume = 60, .duration = 300 }
		                              , { .tone = notes_f3, .volume = 60, .duration = 300 }
		                              , { .tone = notes_e3, .volume = 55, .duration = 300 }
		                              , { .tone = notes_d3, .volume = 55, .duration = 300 }
		                              , { .tone = notes_c3, .volume = 50, .duration = 1200 }
		                              , {0,}
		                              , {0,}
	                              };
	struct slic_oscillator_settings_t settings[ sizeof(tones) / sizeof(struct tone_t)];
	TEST_CHECK(tones_to_oscillator_settings(settings, tones, NULL) *  sizeof(struct tone_t) == sizeof(tones) * sizeof(struct slic_oscillator_settings_t));
}

void TEST_AUTO(calls_test_calls)
{
	{
		struct call_control_t cc;
		const char* call_list = "2,1,0,0,0,\"12345\",,\n3,0,1,0,0,\"67890\"";
		calls_init(cc.calls);
		calls_print(cc.calls);
		TEST_CHECK(calls_from_csv(cc.calls, call_list) == 0);
		TEST_CHECK(calls_count(cc.calls, call_active) == 1);
		TEST_CHECK(calls_count(cc.calls, call_held) == 1);
		TEST_CHECK(cc.calls[1].direction == mobile_terminated);
		TEST_CHECK(cc.calls[1].current.state == call_active);
		TEST_CHECK(cc.calls[1].mode == call_voice);
		TEST_CHECK(!cc.calls[1].current.in_multiparty);
		TEST_CHECK(strcmp(cc.calls[1].number, "12345") == 0);
		TEST_CHECK(cc.calls[2].direction == mobile_originated);
		TEST_CHECK(cc.calls[2].current.state == call_held);
		TEST_CHECK(cc.calls[2].mode == call_voice);
		TEST_CHECK(!cc.calls[2].current.in_multiparty);
		TEST_CHECK(strcmp(cc.calls[2].number, "67890") == 0);
		calls_print(cc.calls);
	}
	{
		struct call_control_t cc;
		const char* call_list = "";
		calls_init(cc.calls);
		calls_print(cc.calls);
		TEST_CHECK(calls_from_csv(cc.calls, call_list) == 0);
		TEST_CHECK(calls_count(cc.calls, call_active) == 0);
		TEST_CHECK(calls_count(cc.calls, call_held) == 0);
		calls_print(cc.calls);
	}
	{
		struct call_control_t cc;
		const char* call_list = "1,1,4,0,0,\"0290117526\",128";
		calls_init(cc.calls);
		calls_print(cc.calls);
		TEST_CHECK(calls_from_csv(cc.calls, call_list) == 0);
		TEST_CHECK(calls_count(cc.calls, call_incoming) == 1);
		TEST_CHECK(cc.calls[0].direction == mobile_terminated);
		TEST_CHECK(cc.calls[0].current.state == call_incoming);
		TEST_CHECK(cc.calls[0].mode == call_voice);
		TEST_CHECK(!cc.calls[0].current.in_multiparty);
		TEST_CHECK(strcmp(cc.calls[0].number, "0290117526") == 0);
		calls_print(cc.calls);
	}
}

void TEST_AUTO(calls_test_call_control)
{
	struct call_control_t cc;
	TEST_CHECK(rdb_open_db() >= 0);
	TEST_CHECK(call_control_init(0, &cc) == 0);
	TEST_CHECK(calls_count(cc.calls, call_active) == 0);
	TEST_CHECK(calls_count(cc.calls, call_held) == 0);
	rdb_close_db();
}

TEST_AUTO_MAIN;

