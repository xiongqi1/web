/*
 * NetComm OMA-DM Client
 *
 * log.c
 * Logging utilities.
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

#define _POSIX_SOURCE

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <logger.h>

#define LARGE_BUFFER_LENGTH 32768

/* Copy an XML dump to the specified log buffer,
 * paying just enough attention to (almost) fix
 * the indentation as we go. */
static void build_xmldump(logger_buf* buf, const char* desc, const char* ptr, int len)
{
    logger_bfwrite(buf, "[XML] %s, %i bytes%c", desc, len, len == 0 ? '.' : ':');

    int  indent  = 0;     /* Current indent count. */
    bool newline = true;  /* Did we just start a new line? */
    bool newtag  = false; /* Did we just enter a new tag? */
    bool intag   = false; /* Are we currently inside a tag? */
    bool hasdata = false; /* Does this block contain data? */
    bool opener  = false; /* Does this tag open a new block? */
    bool closer  = false; /* Does this tag close an existing block? */

    while (len-- > 0 && logger_bcheck(buf, 1))
    {
        if (*ptr == '<') {
            if (!hasdata) {
                newline = true;
                logger_bcwrite(buf, '\n');
                for (int i = -2; i < indent * 2; i++) {
                    logger_bcwrite(buf, ' ');
                }
            }
            newtag = true;
            intag = true;
            opener = true;
            closer = false;
            hasdata = false;
        }
        else if (*ptr == '>') {
            if (intag) {
                intag = false;
                if (opener) {
                    indent++;
                }
                if (closer) {
                    indent--;
                }
            }
        } else if (*ptr == '/') {
            if (intag) {
                opener = false;
                if (newtag) {
                    closer = true;
                }
            }
        } else if (!intag && !isspace(*ptr)) {
            hasdata = true;
        }
        if (*ptr != '\n' && *ptr != '\r') {
            logger_bcwrite(buf, *ptr);
        }
        ptr++;
    }
}

/* Log the contents of a buffer as an indented XML dump. */
void log_xml(int lvl, const char* file, int line, const char* desc, const char* ptr, int len)
{
    if (logger_willprint(lvl, true)) {
        char* msg = malloc(LARGE_BUFFER_LENGTH);
        if (msg) {
            logger_buf buf = LOGGER_BINIT(msg, LARGE_BUFFER_LENGTH);
            build_xmldump(&buf, desc, ptr, len);
            logger_bfinish(&buf);
            __logger(lvl, file, line, true, msg);
            free(msg);
        }
    }
}
