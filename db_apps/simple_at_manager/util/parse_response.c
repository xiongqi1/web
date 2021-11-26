/*
 * A set of functions to parse AT command responses.
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

#include "parse_response.h"

#include "cdcs_syslog.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


//  Attempt to read an integer at the cursor.  If successful this will end up at *result.  The
// value is validity checked and range checked.  If something is wrong then a syslog error is made,
// the exception or mismatch flag is set, and false is returned.
// The char pointers and are ParseResponseState all the same as for process_field() below.
// expected_type is a string purely used for error reporting.
// number_base is 10 for decimals, 16 for hexadecimals.
static bool get_integer(
    ParseResponseState *state,
    const char *source,
    const char *expected_template,
    const char *tch,
    const char **sch,
    long long *result,
    int number_base,
    const char* expected_type,
    long long min_val,
    long long max_val
)
{
    char *end_ptr;
    const long long temp = strtoll(*sch, &end_ptr, number_base);
    bool okay = true;
    if (end_ptr == *sch) {
        SYSLOG_WARNING(
            "Expected a %s <%s> <%s> (%ld chars)",
            expected_type, source, expected_template, (long)(*sch - source)
        );
        okay = false;
    } else if ((temp < min_val) || (temp > max_val)) {
        SYSLOG_WARNING(
            "Integer %lld outside valid range (%lld %lld)  <%s> <%s> %ld"  ,
            temp, min_val, max_val, source, expected_template, (long)(*sch - source)
        );
        okay = false;
    }
    if (!okay) {
        if (state->mismatch) {
            *(state->mismatch) = true;
        } else {
            *(state->exception) = true;
        }
        return false;
    }
    *result = temp;
    *sch = end_ptr;
    return true;
}

// We've got to a "%" in the template string.  As with scanf, this siginifies
// that we have to read a field value from the source string into a variable.
//
// the *pointer* argument points to the variable that we are going to update.
//
//TODO: support the "%%" convention to match a "%" in the source string.
//
// Note that there is provision for a mismatch flag that can be set instead of
// the exception flag for some situations (mismatches).  I haven't implemented
// the setting or checking log for this yet.
static bool process_field(
    ParseResponseState *state,
    const char *source,
    const char *expected_template,
    const char *tch,
    const char **sch,
    void *pointer
)
{
    const char type = tch[1];
    bool okay;
    long long result;

    switch (type) {
        case 'b':
            okay = get_integer(state, source, expected_template, tch, sch, &result, 10, "1 or 0", 0L, 1L);
            if (!okay) {
                return false;
            }
            if (pointer) {
                *(bool*)pointer = result;
            }
            break;
        case 'x':
            okay = get_integer(state, source, expected_template, tch, sch, &result, 16, "hexadecimal", 0L, UINT_MAX);
            if (!okay) {
                return false;
            }
            if (pointer) {
                *(unsigned*)pointer = result;
            }
            break;
        case 'd':
            okay = get_integer(state, source, expected_template, tch, sch, &result, 10, "decimal", INT_MIN, INT_MAX );
            if (!okay) {
                return false;
            }
            if (pointer) {
                *(int*)pointer = result;
            }
            break;
        case 'l':
            okay = get_integer(state, source, expected_template, tch, sch, &result, 16, "long long hex", 0L, LLONG_MAX);
            if (!okay) {
                return false;
            }
            if (pointer) {
                *(long long unsigned*)pointer = result;
            }
            break;
        case 's':
            {
                // a string - delimited by whatever character follows the "%s"
                // in the format string.  If it is a NUL (the end of format
                // string then the input string will extend to the end of the
                // source string.
                char **str_value = pointer;
                char delimiter = tch[2];
                if (delimiter == '\'') {
                    delimiter = '"';
                }
                if (pointer) {
                    *str_value = state->current;
                }
                while (**sch != delimiter) {
                    if (*sch == '\0') {
                        if (state->mismatch) {
                            *(state->mismatch) = true;
                        } else {
                            SYSLOG_WARNING(
                                "Hit NUL reading string field <%s> <%s> %ld",
                                source, expected_template,
                                (long)(*sch - source)
                            );
                            *(state->exception) = true;
                        }
                        return false;
                    }
                    if (pointer && (state->current >= state->end - 2)) {
                        SYSLOG_WARNING(
                            "Buffer Overflow <%s> <%s> %ld",
                            source, expected_template, (long)(*sch - source)
                        );
                        *(state->exception) = true;
                        return false;
                    }
                    if (pointer) {
                        *(state->current)++ = *(*sch)++;
                    }
                }
                if (pointer) {
                    *(state->current)++ = '\0';
                }
            }
            break;
        case '\0':
            SYSLOG_WARNING("Trailing %% <%s>", expected_template);
            *(state->exception) = true;
            return false;

        default:
            SYSLOG_WARNING("Bad var type <%c> <%s>", type, expected_template);
            *(state->exception) = true;
            return false;
    }
    return true;
}


// Refer to include file for details.
ParseResponseState set_parse_response_buffer(
    char *start,
    size_t size,
    bool *exception
)
{
    ParseResponseState state = {
        .start = start,
        .current = start,
        .end = start + size,
        .exception = exception,
        .mismatch = NULL  // Not used yet.
    };
    return state;
}


// Refer to include file for details.
//
void reset_parse_response_buffer(ParseResponseState *state)
{
    state->current = state->start;
}


// Refer to include file for details.
//
// Really just a variadic function wrapper for parse_response_v().
void parse_response(
    ParseResponseState *state,
    const char *source,
    const char *expected_template,
    ...
)
{
    va_list args;
    va_start(args, expected_template);
    parse_response_v(state, source, expected_template, args);
    va_end(args);
}



// Refer to include file for details.
//
void parse_response_v(
    ParseResponseState *state,
    const char *source,
    const char *expected_template,
    va_list args
)
{
    // If ther is a pre-existing fault then bail without touching anything.
    if (*(state->exception)) {
        return;
    }
    const char *tch = expected_template;
    const char *sch = source;

    // Loop until we either run out of template string, or out of source string;
    // hopefully they both end at the same point.
    while ((*tch != '\0') && (*sch != '\0')) {
        // Check the current template character and decide what to do.
        switch (*tch) {
        case '\'':
            // A <'> has to match to a <"> in the source.
            if (*sch != '"') {
                if (state->mismatch) {
                    *(state->mismatch) = true;
                } else {
                    SYSLOG_WARNING(
                        "Mismatch quote <%s> <%s> %ld",
                        source, expected_template, (long)(sch - source)
                    );
                    *(state->exception) = true;
                }
                return;
            }
            tch++;
            sch++;
            break;
        case '%':
            {
                // A <%> in the template means we are going to read in a field
                // or we expect a <%> in the source.

                // All of our variadic arguments are pointers, just read from
                // args here to avoid issues with use of the va_arg() macro
                // in process_field()

                void *pointer = va_arg(args, void*);
                const bool okay = process_field(
                    state, source, expected_template, tch, &sch, pointer
                );
                if (!okay) {
                    // Failure: either exception or mismatch have already been
                    // set.
                    return;
                }
                tch += 2;
            }
            break;

        default:
            // Just a normal character in the template - must correspond exactly
            // with one in the source.
            if (*sch != *tch) {
                if (state->mismatch) {
                    *(state->mismatch) = true;
                } else {
                    SYSLOG_WARNING(
                        "Mismatch char <%c> <%s> <%s> %ld",
                        *tch, source, expected_template, (long)(sch - source)
                    );
                    *(state->exception) = true;
                    return;
                }
            }
            tch++;
            sch++;
            break;
        }
    }

    // Check that we got to the end of the template at the same point we got to
    // the end of the source.
    if ((*sch != '\0') || (*tch != '\0')) {
        if ((state->mismatch)) {
            *(state->mismatch) = true;
        } else {
            SYSLOG_WARNING(
                "Premature end of source <%s> <%s>", source, expected_template
            );
            *(state->exception) = true;
        }
    }
}

