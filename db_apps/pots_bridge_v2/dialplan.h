#ifndef __DIALPLAN_H_20180709__
#define __DIALPLAN_H_20180709__

/*
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

#include "uthash.h"
#include "utlist.h"
#include <regex.h>

#define DIALPLAN_OPT_REGEX "\\\\[0-9]+"
#define DIALPLAN_DELIMITER ":"

#define DIALPLAN_MAX_OPTION_COUNT 10
#define DIALPLAN_MAX_PLAN_LEN 1024

/* dial plan entry */
struct dialplan_plan_entry_t {
    int priority; /* dial plan priority */
    char* regex; /* dial plan regular expression */
    char* func_name; /* dial plan function name */
    char* opt; /* dial plan option */

    regmatch_t rm_opt[DIALPLAN_MAX_OPTION_COUNT]; /* array of option regmatch */
    int opt_index[DIALPLAN_MAX_OPTION_COUNT]; /* array of option indexes */

    regex_t re; /* compiled regular expression */

    struct dialplan_plan_entry_t* next;
};


typedef int (*dialplan_func_t)(const char* func_name, const char* opt, void* ref);

/* dial plan function entry */
struct dialplan_func_entry_t {
    const char* func_name; /* dial plan function name */
    dialplan_func_t func_ptr; /* dial plan function */

    UT_hash_handle hh; /* hash handle */
};

#define dialplan_for_each_plan_begin(pent) \
    { \
        struct dialplan_plan_entry_t* temp; \
        LL_FOREACH_SAFE(dialplan_get_plan_head(),pent,temp) {

#define dialplan_for_each_plan_end() \
    } \
    }


int dialplan_add_func(const char* func_name, dialplan_func_t func_ptr);
int dialplan_init(void);
int dialplan_match_and_call_plan(int priority, const char* num, void* ref, int* dialplan_call_rc);
int dialplan_parse_and_add_plan(const char *plan);
void dialplan_fini(void);
void dialplan_sort_plan(void);
int dialplan_call_func_by_name(const char *func_name, const char* opt, void *ref);

#endif
