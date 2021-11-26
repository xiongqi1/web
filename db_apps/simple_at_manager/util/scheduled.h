/*!
 * Copyright Notice:
 * Copyright (C) 2008 Call Direct Cellular Solutions Pty. Ltd.
 *
 * Provide a crude task scheduing system for the simple_at_manager.
 *
 * scheduled_fire() is called from within the main loop.  This seems to currently be on a one second
 * interval.
 *
 * Repetition is achieved by calling scheduled_func_schedule() from within the callback it triggers.
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

#ifndef SIMPLE_AT_MANAGER_SCHEDULED_H_
#define SIMPLE_AT_MANAGER_SCHEDULED_H_

// Call once before using the system.  Done from within the main start-up code.
void scheduled_init( void );

// Arrange for model_run_command() to be run every duration_sec seconds.  It will be passed
// name and value as array parameters at that point.
int scheduled_schedule( const char* name, const char* value, unsigned int duration_sec );

// Lodge a function to be executed every duration_sec seconds.
// The name is used to track the task, for example to call scheduled_clear() to remove it.
// The function should accept a single parameter, a pointer.  This appears to point to the
// internally defined data structure for the schedule item.
// Presumably this is to allow some function functionality.
// Calling for an existing schedule with change that time.
int scheduled_func_schedule(const char* name,void (*func)(void* ref), unsigned int duration_sec);

// Lodge a function to be executed once in duration_sec seconds' time.
// The name is used to track the task, for example to call scheduled_clear() to remove it.
// The pending callback can be cancelled with scheduled_clear().
int one_shot_func_schedule(const char* name,void (*func)(void* ref), unsigned int duration_sec);

// Cancel a pending callback to a function.
void scheduled_clear( const char* name );

// Check to see if any tasks are pending and, if so, execute them.  Called from within the main loop
// at a one second interval.
void scheduled_fire( void );

#endif /* SIMPLE_AT_MANAGER_SCHEDULED_H_ */

