/*
 * A set of functions to safely build strings.
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

#include "sprintf_buffer.h"

#include "cdcs_syslog.h"

#include <stdarg.h>
#include <stdio.h>


// Refer to comment in include file.
SprintfBufferState sprintf_buffer_start(
    char *buffer,
    size_t size,
    bool *exception
)
{
    SprintfBufferState state = {
        .start = buffer,
        .current = buffer,
        .end = buffer + size,
        .exception = exception
    };
    return state;
}

// Refer to comment in include file.
int sprintf_buffer_add(SprintfBufferState *state, const char *format, ...)
{
    // If an exception's been flagged prior to this call then do nothing.
    if (*(state->exception)) {
        return 0;
    }

    // Call the safe sprintf function to do all the actul work, noting the
    // return value.
    va_list args;
    va_start(args, format);
    const int chars_out = vsnprintf(
        state->current,
        state->end - state->current,
        format,
        args
    );
    va_end(args);

    // Check the return value first for an error code ...
    if (chars_out < 0) {
        SYSLOG_ERR("sprintf error: '%s', %d", format, chars_out);
        *(state->exception) = true;
        return 0;
    }

    // ... and then check if the number of characters vsnprintf() wanted to
    // write exceeded the remaining buffer size.
    if (chars_out + state->current >= state->end) {
        SYSLOG_ERR("sprintf buffer overrun: '%s', '%s'", format, state->start);
        *(state->exception) = true;
        return 0;
    }

    // The vsnprintf() call succeeded; advance our cursor.
    state->current += chars_out;

    // The total characters written (not just on this call).
    return state->current - state->start;
}


// Refer to comment in include file.
const char *sprintf_buffer(SprintfBufferState *state)
{
    return state->start;
}


// Refer to comment in include file.
void reset_sprintf_buffer(SprintfBufferState *state)
{
    state->current = state->start;
}
