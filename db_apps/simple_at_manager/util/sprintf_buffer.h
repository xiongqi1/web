#ifndef SPRINTF_BUFFER_H_12000015002018
#define SPRINTF_BUFFER_H_12000015002018
/*
 * A set of lightweight functions to safely build strings.
 *
 * Lets us construct a character array for one or more calls to a printf
 * format conversant function.  Handles all the buffer overflow checks in the
 * background.  If an overflow occurs a SYSLOG message is generated and a
 * user specified exception flag is set.  If this flag was already set before
 * routine is called then it returns without doing anything.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Wireless Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS LIMITED ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>
#include <stdbool.h>


// This structure maintains the state of a sprintf_buffer "object".  Clients
// shouldn't need to access its internals directly.
typedef struct {
    char *start;
    char *end;
    char *current;
    bool *exception;
} SprintfBufferState;


// The constructor - if you like.
// Assigns a character buffer (with a specified size) to the object.
// This is where the string will be constructed.
//
// In addition a pointer to an exception flag is passed.  This flag is checked
// and - if warranted - set by sprintf_buffer_add().
//
// Returns the state structure that is referenced by sprintf_buffer_add()
SprintfBufferState sprintf_buffer_start(
    char *buffer,
    size_t size,
    bool *exception
);


// Builds the string using a printf() style format string and variadic
// arguments.
//
// The first argument is the state structure created by sprintf_buffer_start()
//
// The function use a format string identical to printf and friends.  (It passes
// the format string and trailing arguments unmolested to vsnprintf().)
//
// The exception flag referenced in the state variable is checked before we do
// anthing and will trigger an immediate return is set.
// If a write would cause the buffer to overflow (more state->size characters)
// then the exception flag is set, an error message sent to syslog and the
// function returns.
//
// If called multiple times, the function appends each string after the last.
//
// To be consistent with the printf family return the number of characters
// written so far to the buffer (this as preceding calls).
int sprintf_buffer_add(SprintfBufferState *state, const char *format, ...);


// Return a pointer to the string constructed.  This will just be the address of
// the buffer passed to sprintf_buffer_start().
const char *sprintf_buffer(SprintfBufferState *state);


// Clears the buffer so a new string can be constructed in it.  The exception
// flag is not reset however.
void reset_sprintf_buffer(SprintfBufferState *state);


#endif        //  #ifndef SPRINTF_BUFFER_H_12000015002018
