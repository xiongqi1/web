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

#include <stdio.h>
#include <string.h>
#include <sys/times.h> 
#include <unistd.h>
#include <stdbool.h>

#include "cdcs_syslog.h"

#include "../model/model.h"
#include "./scheduled.h"

#define SCHEDULED_NAME_SIZE 64 // increase, if not enough
#define SCHEDULED_VALUE_SIZE 32 // increase, if not enough
#define SCHEDULED_VARIABLES_SIZE 10 // increase, if not enough

struct scheduled_t
{
	clock_t start_clk;
	clock_t duration;
	void (*func)(void* ref);
	char name[SCHEDULED_NAME_SIZE];
	char value[SCHEDULED_VALUE_SIZE];
	bool one_shot;
};

static struct scheduled_t scheduled_variables_[SCHEDULED_VARIABLES_SIZE];

static long clkPerSec;

void scheduled_init(void)
{
	int i;
	for (i = 0; i < SCHEDULED_VARIABLES_SIZE; ++i)
	{
		scheduled_variables_[i].name[0] = 0;
	}

	clkPerSec=sysconf(_SC_CLK_TCK);
}


void scheduled_clear(const char* name)
{
	int i;
	for (i = 0; i < SCHEDULED_VARIABLES_SIZE; ++i)  // quick and dirty, improve, if too slow
	{
		if (strcmp(scheduled_variables_[i].name, name) == 0)
		{
			scheduled_variables_[i].name[0] = 0;
			return;
		}
	}
}


// Does the heavy lifting for the publicly facing schedule setting functions.
// Tries to create or update a schedule item. If name alrady exists in scheduled_variables_ then
// that slot is updated.  Otherwise a an empty slot is sought to create the new schedule definition
// in.  The schedule period is set to duration_sec seconds.  If one-shot is true then the event
// occurs just the once and then the schedule item is removed.  If it is false then the event will
// continue to trigger until scheduled_clear() is called on it.
static int scheduled_common_schedule(
    const char* name,
    const char* value,
    void (*func)(void* ref),
    unsigned int duration_sec,
    bool one_shot)
{
	int i;
	struct scheduled_t* scheduled = NULL;
	struct tms tmsbuf;
	
	if (duration_sec == 0)
	{
		SYSLOG_ERR("expected duration in seconds, got 0");
		return -1;
	}
	if (strlen(name) > SCHEDULED_NAME_SIZE - 1)
	{
		SYSLOG_ERR("'%s' too long, can schedule names not longer than %d bytes", name, SCHEDULED_NAME_SIZE - 1);
		return -1;
	}
	if (strlen(value) > SCHEDULED_VALUE_SIZE - 1)
	{
		SYSLOG_ERR("'%s' too long, can schedule values not longer than %d bytes", value, SCHEDULED_VALUE_SIZE - 1);
		return -1;
	}

	// Check for the name first
	for (i = 0; (i < SCHEDULED_VARIABLES_SIZE) && !scheduled; ++i)  // quick and dirty, improve, if too slow
	{
		if (strcmp(scheduled_variables_[i].name, name) == 0)
		{
			scheduled = scheduled_variables_ + i;
		}
	}
	// if we can't find the name then check for an empty slot
	if (!scheduled) {
        for (i = 0; (i < SCHEDULED_VARIABLES_SIZE) && !scheduled; ++i)  // quick and dirty, improve, if too slow
        {
            if (scheduled_variables_[i].name[0] == '\0')
            {
                scheduled = scheduled_variables_ + i;
            }
        }
    }
	if (!scheduled)
	{
		SYSLOG_ERR("failed to schedule '%s', can schedule not more than %d events", name, SCHEDULED_VARIABLES_SIZE);
		return -1;
	}

	strcpy(scheduled->name, name);
	strcpy(scheduled->value, value);

	scheduled->start_clk=times(&tmsbuf);
	scheduled->duration = duration_sec*clkPerSec;
	scheduled->func=func;
	scheduled->one_shot = one_shot;

	return 0;
}

int scheduled_schedule(const char* name, const char* value, unsigned int duration_sec)
{
	return scheduled_common_schedule(name,value,0,duration_sec, false);
}

int scheduled_func_schedule(const char* name,void (*func)(void* ref), unsigned int duration_sec)
{
	return scheduled_common_schedule(name,"",func,duration_sec, false);
}

int one_shot_func_schedule(const char* name,void (*func)(void* ref), unsigned int duration_sec)
{
	return scheduled_common_schedule(name,"",func,duration_sec, true);
}


void scheduled_fire(void)
{
	int i;
	clock_t now;
	struct name_value_t args[2];
	struct tms tmsbuf;

	now=times(&tmsbuf);

	for (i = 0; i < SCHEDULED_VARIABLES_SIZE; ++i)  // quick and dirty, improve, if too slow
	{
		if(now - scheduled_variables_[i].start_clk<scheduled_variables_[i].duration)
			continue;

		if (scheduled_variables_[i].name[0] == 0)
			continue;

		scheduled_variables_[i].start_clk=now;

		if(scheduled_variables_[i].func)
		{
			scheduled_variables_[i].func(&scheduled_variables_[i]);
		}
		else
		{
			args[0].name = scheduled_variables_[i].name;
			args[0].value = scheduled_variables_[i].value;
			args[1].name = NULL; // zero-terminator
			args[1].value = NULL; // zero-terminator

			if (model_run_command(args,0) != 0)
				SYSLOG_ERR("'%s' '%s' failed", args[0].name, args[0].value);
		}

		if (scheduled_variables_[i].one_shot) {
		    // Now it's fired prevent repeats.
		    scheduled_variables_[i].name[0] = '\0';
		}
	}
}
