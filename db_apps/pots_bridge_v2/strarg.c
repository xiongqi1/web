/*
 * strarg - string argument manager.
 *
 * Copyright Notice:
 * Copyright (C) 2018 NetComm Limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Limited.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM LIMITED ``AS IS''
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


#include "strarg.h"

#include <stdlib.h>
#include <syslog.h>
#include <string.h>

#define STRARG_ESC      '\\'
#define STRARG_DELIM	' '

/**
 * @brief destroys string argument object.
 *
 * @param a is string argument object.
 */
void strarg_destroy(struct strarg_t* a)
{
    if (!a)
        return;

    free(a->arg);
    free(a);
}

/**
 * @brief creates string argument object.
 *
 * @param arg is argument string.
 *
 * @return
 */
struct strarg_t* strarg_create(const char* arg)
{
    struct strarg_t* a;

    if (!(a = calloc(1, sizeof(*a)))) {
        syslog(LOG_DEBUG, "###channel### cannot create strarg");
        goto err;
    }

    if (!(a->arg = strdup(arg))) {
        syslog(LOG_DEBUG, "###channel### cannot duplicate string (arg=%s)", arg);
        goto err;
    }

    /* broken arguments */
    int di = 0;
    int si = 0;
    int starti = 0;
    int esc = 0;
    int argi = 0;
    char ch;
    while (((ch = arg[si]) != 0) && (di < STRARG_LINEBUFF_SIZE) && (argi < STRARG_ARGMENT_SIZE)) {

        /* if first escape char */
        if (!esc && (ch == STRARG_ESC)) {
            esc = 1;

            si++;
        } else {
            /* if in escape sequence or not delim */
            if (esc || (ch != STRARG_DELIM)) {
                a->line[di++] = ch;
                si++;
            }
            /* if delimi */
            else if (ch == STRARG_DELIM) {
                a->line[di++] = 0;
                si++;

                /* get token */
                a->argv[argi++] = &a->line[starti];
                /* get new start of string */
                starti = di;
            }

            esc = 0;
        }
    }

    /* get left-over token */
    if ((starti != di) || !starti) {
        a->line[di++] = 0;
        /* store start of string */
        a->argv[argi++] = &a->line[starti];
    }

    /* get count */
    a->argc = argi;

    return a;

err:
    strarg_destroy(a);
    return NULL;
}

#ifdef CONFIG_UNIT_TEST

#include <stdio.h>

int main(int argc, char argv[])
{
    struct strarg_t* a;
    int i;

    a = strarg_create("a   b c d\\ e\\ f\\\\ abcd ");

    for (i = 0; i < a->argc; i++) {
        printf("%d : '%s'\n", i, a->argv[i]);
    }

    strarg_destroy(a);

}

#endif
