#ifndef PARSE_RESPONSE_H_12000015002018
#define PARSE_RESPONSE_H_12000015002018
/*
 * A set of functions to parse strings such as AT command responses.
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

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>


// This structure contains the state of a ParseResponse "object".  It is passed
// to the variable functions.  The internals ought not be accessed directly.
typedef struct {
    char *start;  // first char in the buffer
    char *end; // first char past the buffer
    char *current; // where we would next write to in the buffer
    bool *exception; // pointer to the global exceptio that we check and set
    bool *mismatch; // pointer to a bool that we might set instead of exception
} ParseResponseState;


// Set up the character buffer used by parse_responseX().
//
// Think "constructor".
//
// This needs to be done before either of the parse_responseX() functions are
// called.  The buffer needs to be big enough to hold all the strings we might
// extract from a response.  A safe choice is to made it the same size as the
// maximum response string.
//
// It is passed a pointer to a character buffer that we can use for storage of
// string fields extracted from the source string.  *Size* is the length of this
// buffer (we will not excede this).
//
// This is also passed a pointer to an exception flag that is checked prior to
// parse_responseX() doing anything, and set if there's an issue.
//
// The function may be safely called before each call to parse_responseX().
//
// The internal variables should not need to be accessed by client code.
ParseResponseState set_parse_response_buffer(
    char *start,
    size_t size,
    bool *exception
    );


// Clears the buffer pointer so parse_responseX() can be recalled indefinately
// without risking overflowing the buffer.
void reset_parse_response_buffer(ParseResponseState *state);


// Parse a string (such as an AT command response) for field values.
//
// - state is the "object" created by set_parse_response_buffer().
// - source is the string to parse
// - expected_template is the form we expect the string to be.  It has scanf()
//   like formatting to mark where the readable fields lie.
//   Note though that the formatting differs from scanf
// %b a 0 or a 1, the corresponding argument is a (bool*)
//
// %d decimal integer, the corresponding argument is an (int *)
//
// %x hexadecimal integer, the corresponding argument is an (unsigned *)
//
// %l a 64 bit bit hexadecimal integer, the corresponding argument is a (long long unsigned *)
//
// %s string, delimited by the character following the 's' in the template.
//    So "%s" will set the corresponding string pointer to a string containing
//    everything between the two quotes - a (char**).
//
// Note that if the pointer to any read variable is NULL then the value is still read but discarded.
// This is a means of skipping over values we want to ignore.
//
// Note that all <'> characters in the template string are mapped to <"> strings
// in the incoming text.  This is done to aid with readiblity in the client code
// (We can avoid backslashing quotes as in "this is a \"quote\".".)
//
// If the exception flag (referenced in set_parse_response_buffer()) is already
// set when the function is called then it exits without doing a thing.
// If any fault occurs during operation - such as a mismatch between the format
// string and the source string then the exception flag is set.
void parse_response(
    ParseResponseState *state,
    const char *source,
    const char *expected_template,
    ...
);


// Same as parse_response() above but with the field arguments bundled into
// a varadic argument list.
void parse_response_v(
    ParseResponseState *state,
    const char *source,
    const char *expected_template,
    va_list args
);

#endif        //  #ifndef PARSE_RESPONSE_H_12000015002018
