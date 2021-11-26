/*
 * Dialplan matches phone numbers to dial plans.
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

#include "dialplan.h"
#include "utils.h"
#include "uthash.h"
#include <errno.h>
#include <linux/stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static struct dialplan_plan_entry_t* _plan_head = NULL;
static struct dialplan_func_entry_t* _func_head = NULL;

static regex_t _re_opt = {0,};

/**
 * @brief priority sort compare function.
 *
 * @param a is dial plan entry to compare.
 * @param b is dial plan entry to compare.
 *
 * @return substation result of b->priority to a->priority.
 */
static int dialplan_sort_comp(struct dialplan_plan_entry_t* a, struct dialplan_plan_entry_t* b)
{
    return a->priority - b->priority;
}

/**
 * @brief obtains dial plan header of dial plan list.
 *
 * @return dial plan header.
 */
static struct dialplan_plan_entry_t* dialplan_get_plan_head(void)
{
    return _plan_head;
}

/**
 * @brief sorts dial plans in dial plan list.
 */
void dialplan_sort_plan(void)
{
    LL_SORT(_plan_head, dialplan_sort_comp);
}

/**
 * @brief remove a dial plan from dial plan list.
 *
 * @param pent is a dial plan entry to remove.
 */
static void dialplan_del_plan(struct dialplan_plan_entry_t* pent)
{
    if (!pent) {
        goto fini;
    }

    LL_DELETE(_plan_head, pent);

    free(pent->regex);
    free(pent->func_name);
    free(pent->opt);
    regfree(&pent->re);
    free(pent);

fini:
    return;
}


/**
 * @brief obtains matched string.
 *
 * @param rm is an array of regmatch structures.
 * @param idx is index of rm structures.
 * @param src is source string.
 *
 * @return
 */
static const char* __get_rm_str_by_index(regmatch_t* rm, int idx, const char* src)
{
    int s;
    int e;
    int i;

    static char dst[DIALPLAN_MAX_PLAN_LEN];

    s = rm[idx].rm_so;
    e = rm[idx].rm_eo;

    if (s < 0) {
        goto err;
    }

    /* copy arguments */
    i = 0;
    while ((s + i < e) && (i < DIALPLAN_MAX_PLAN_LEN - 1)) {
        dst[i] = src[s + i];
        i++;
    }

    /* set NUL terminatino */
    dst[i] = 0;

    return dst;

err:
    return NULL;
}


/**
 * @brief performs regexec multiple times.
 *
 * @param re is compiled regular expression.
 * @param str is source srouce.
 * @param nrm is total number of rm.
 * @param rm is an array of regmatch.
 * @param eflags eflags for regexec().
 *
 * @return return result from regexec().
 */
static int regexec_multiple(const regex_t* re, const char* str, int nrm, regmatch_t* rm, int eflags)
{
    int i;
    int src_s;
    int stat = REG_EEND;
    int src_len = strlen(str);

    regmatch_t* rm_cur;

    src_s = 0;
    for (i = 0; (i < nrm) && (src_s < src_len); i++) {
        rm_cur = rm + i;
        stat = regexec(re, str + src_s, 1, rm_cur, eflags);

        /* break if we get an error */
        if (stat != REG_NOERROR) {
            break;
        }

        /* change offset to be relative from the beginning */
        rm_cur->rm_so += src_s;
        rm_cur->rm_eo += src_s;

        src_s = rm_cur->rm_eo;
    }

    /* set EOF */
    if (i < nrm) {
        rm[i].rm_so = -1;
    }

    return stat;
}

/**
 * @brief adds a dial plan.
 *
 * @param priority is dial plan priority.
 * @param regex is regular expression.
 * @param func_name is dial plan function name.
 * @param opt is option string for dial plan function .
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
static int dialplan_add_plan(int priority, const char* regex, const char* func_name, const char* opt)
{
    struct dialplan_plan_entry_t* pent = NULL;
    int stat;
    const char* opt_temp;

    int i;

    ///////////////////////////////////////////////////////////////////////////////
    // prepare plane entry
    ///////////////////////////////////////////////////////////////////////////////

    pent = calloc(1, sizeof(*pent));
    if (!pent) {
        syslog(LOG_ERR, "failed to allocate memory for dialplan entry");
        goto err;
    }

    /* initiate dialplan plan entry */
    pent->priority = priority;
    pent->regex = strdup(regex);
    pent->func_name = strdup(func_name);
    pent->opt = strdup(opt);

    if (!pent->regex || !pent->func_name || !pent->func_name || !pent->opt) {
        syslog(LOG_ERR, "failed to allocate memory for diallan entry's member");
        goto err;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // prepare opt regex match and opt index
    ///////////////////////////////////////////////////////////////////////////////
    regexec_multiple(&_re_opt, pent->opt, DIALPLAN_MAX_OPTION_COUNT, pent->rm_opt, 0);

    /* obtain opt index */
    for (i = 0; i < DIALPLAN_MAX_OPTION_COUNT; i++) {

        if (pent->rm_opt[i].rm_so < 0) {
            break;
        }

        opt_temp = __get_rm_str_by_index(pent->rm_opt, i, pent->opt);
        if (!opt_temp) {
            break;
        }

        pent->opt_index[i] = atoi(opt_temp + 1);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // compile plan regex
    ///////////////////////////////////////////////////////////////////////////////

    /* compile regex */
    stat = regcomp(&pent->re, pent->regex, REG_EXTENDED);
    if (stat != REG_NOERROR) {
        syslog(LOG_ERR, "failed to compile regex (regex='%s')", pent->regex);
        goto err;
    }

    LL_APPEND(_plan_head, pent);

    return 0;

err:
    dialplan_del_plan(pent);

    return -1;
}

/**
 * @brief deletes a dial plan function.
 *
 * @param fent is dial plan function entry to add.
 */
static void dialplan_del_func(struct dialplan_func_entry_t* fent)
{
    if (!fent) {
        goto fini;
    }

    HASH_DEL(_func_head, fent);
    free(fent);

fini:
    return;
}

/**
 * @brief adds a dial plan function.
 *
 * @param func_name is function name to add.
 * @param func_ptr is function pointer to add.
 *
 * @return 0 when it succeeds. Otherwise, -1.
 */
int dialplan_add_func(const char* func_name, dialplan_func_t func_ptr)
{
    struct dialplan_func_entry_t* fent = NULL;

    fent = malloc(sizeof(*fent));
    if (!fent) {
        syslog(LOG_ERR, "failed to allocate memory for diaplan entry");
        goto err;
    }

    /* initiate dialplan function entry */
    fent->func_name = func_name;
    fent->func_ptr = func_ptr;

    HASH_ADD_STR(_func_head, func_name, fent);

    return 0;

err:
    dialplan_del_func(fent);

    return -1;
}

/**
 * @brief initializes dial plan module.
 *
 * @return
 */
int dialplan_init(void)
{
    int stat;

    /* compile opt regex */
    stat = regcomp(&_re_opt, DIALPLAN_OPT_REGEX, REG_EXTENDED);
    if (stat != REG_NOERROR) {
        syslog(LOG_ERR, "failed to compile option regex - %s", strerror(errno));
        goto err;
    }

    return 0;

err:
    return -1;
}

/**
 * @brief finalizes dial plan modules.
 */
void dialplan_fini(void)
{
    struct dialplan_plan_entry_t* pent;
    struct dialplan_func_entry_t* fent;
    struct dialplan_func_entry_t* tmp;

    /* delete plans */
    dialplan_for_each_plan_begin(pent) {
        dialplan_del_plan(pent);
        dialplan_for_each_plan_end();
    }

    /* delete functions */
    HASH_ITER(hh, _func_head, fent, tmp) {
        dialplan_del_func(fent);
    }

    regfree(&_re_opt);
}


/**
 * @brief calls a dial plan function.
 *
 * @param fent s function entry to call.
 * @param opt is option for dial function.
 * @param ref is reference data for dial function.
 *
 * @return result from dial function.
 */
static int dialplan_call_func(struct dialplan_func_entry_t* fent, const char* opt, void *ref)
{
    return fent->func_ptr(fent->func_name, opt, ref);
}

/**
 * @brief checks to see if phone number matches a dial plan.
 *
 * @param pent is a dial plan entry to check.
 * @param num is phone number string.
 * @param fent_out is function entry as result.
 *
 * @return option string for function entry.
 */
static const char* dialplan_match_plan(struct dialplan_plan_entry_t* pent, const char* num,
                                       struct dialplan_func_entry_t** fent_out)
{
    int stat;

    regmatch_t rm[DIALPLAN_MAX_OPTION_COUNT];
    char* opt[DIALPLAN_MAX_OPTION_COUNT] = {NULL,};
    int i;
    const char* opt_temp;

    static char dst[DIALPLAN_MAX_PLAN_LEN];
    int src_s;
    char dst_s;
    int src_len;
    int dst_len;
    int len;

    int stat_printf;
    const char* res = NULL;

    regmatch_t* rm_opt;

    int opt_index;

    struct dialplan_func_entry_t* fent;

    ///////////////////////////////////////////////////////////////////////////////
    // execute regex on num
    ///////////////////////////////////////////////////////////////////////////////

    /* execute regex on num */
    stat = regexec(&pent->re, num, DIALPLAN_MAX_OPTION_COUNT, rm, 0);

    /* bypass if nothing is matched */
    if (stat == REG_NOMATCH) {
        goto fini;
    }

    /* bypass if there is errors */
    if (stat != REG_NOERROR) {
        syslog(LOG_ERR, "failed in matching regular express for num (regex=%s,err=%s)", pent->regex, strerror(errno));
        goto fini;
    }

    /* bypass if function does not exist */
    HASH_FIND_STR(_func_head, pent->func_name, fent);
    if (!fent) {
        syslog(LOG_ERR, "failed to find function (func='%s')", pent->func_name);
        goto fini;
    }

    *fent_out = fent;

    ///////////////////////////////////////////////////////////////////////////////
    // build opt contents
    ///////////////////////////////////////////////////////////////////////////////

    /* obtain opt contents */
    for (i = 0; i < DIALPLAN_MAX_OPTION_COUNT; i++) {
        if (rm[i].rm_so < 0) {
            break;
        }

        opt_temp = __get_rm_str_by_index(rm, i, num);
        if (!opt_temp) {
            break;
        }

        opt[i] = strdup(opt_temp);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // render opt
    ///////////////////////////////////////////////////////////////////////////////

    dst_s = 0;
    src_s = 0;
    for (i = 0; i < DIALPLAN_MAX_OPTION_COUNT; i++) {

        rm_opt = &pent->rm_opt[i];

        /* render last part of opt and break */
        if (rm_opt->rm_so < 0) {

            dst_len = sizeof(dst) - dst_s;
            snprintf(dst + dst_s, dst_len, "%s", pent->opt + src_s);

            break;
        }

        /* get source starting index and length */
        src_len = rm_opt->rm_so - src_s;
        /* get dst length */
        dst_len = sizeof(dst) - dst_s - 1;
        len = min(dst_len, src_len);

        /* copy body */
        if (len) {
            stat_printf = snprintf(dst + dst_s, len + 1, "%s", pent->opt + src_s);
            if (stat_printf < 0) {
                syslog(LOG_ERR, "failed in calling snprintf() - %s", strerror(errno));
                goto fini;
            }
        }

        /* increase destination */
        dst_s += len;

        /* get opt index */
        opt_index = pent->opt_index[i];

        /* get source len */
        src_len = opt[opt_index] ? strlen(opt[opt_index]) : 0;
        /* get dst length */
        dst_len = sizeof(dst) - dst_s - 1;
        len = min(dst_len, src_len);

        /* render opt */
        if (len) {
            stat_printf = snprintf(dst + dst_s, len + 1, "%s", opt[opt_index]);
            if (stat_printf < 0) {
                syslog(LOG_ERR, "failed in calling snprintf() - %s", strerror(errno));
                goto fini;
            }
        }
        /* increase destination and source */
        dst_s += len;
        src_s = rm_opt->rm_eo;
    }

    res = dst;

fini:
    /* free opt values */
    for (i = 0; i < DIALPLAN_MAX_OPTION_COUNT; i++) {
        free(opt[i]);
    }

    return res;

}

/**
 * @brief parses and adds a dial plan.
 *
 * @param plan is a paln that is colon(:) separated
 *
 * @return 0 when succeeds. Otherwise, -1.
 */
int dialplan_parse_and_add_plan(const char* plan)
{

    /*

     priority / regular expression / function name / args

    */

    char delimit[] = DIALPLAN_DELIMITER;
    char* priority_str;
    int priority;
    char* regex;
    char* func_name;
    char* opt;

    char line[DIALPLAN_MAX_PLAN_LEN];

    static struct dialplan_func_entry_t* fent;

    snprintf(line, sizeof(line), "%s", plan);

    if (!*line) {
        syslog(LOG_ERR, "blank plan found");
        goto err;
    }

    /* parse plan */
    priority_str = strtok(line, delimit);
    if (!priority_str) {
        syslog(LOG_ERR, "priority missing (plan='%s')", plan);
        goto err;
    }

    priority = atoi(priority_str);
    if (!priority) {
        syslog(LOG_ERR, "incorrect priority (plan='%s')", plan);
        goto err;
    }

    regex = strtok(NULL, delimit);
    if (!regex) {
        syslog(LOG_ERR, "regular expression missing (plan='%s')", plan);
        goto err;
    }

    func_name = strtok(NULL, delimit);
    if (!func_name) {
        syslog(LOG_ERR, "function missing (plan='%s')", plan);
        goto err;
    }

    opt = strtok(NULL, delimit);
    if (!opt)
        opt = "";

    HASH_FIND_STR(_func_head, func_name, fent);
    if (!fent) {
        syslog(LOG_ERR, "registered function not found (plan='%s',func=%s)", plan, func_name);
        goto err;
    }

    return dialplan_add_plan(priority, regex, func_name, opt);

err:
    return -1;
}


int dialplan_call_func_by_name(const char *func_name, const char* opt, void *ref)
{
    static struct dialplan_func_entry_t* fent;

    /* bypass if function does not exist */
    HASH_FIND_STR(_func_head, func_name, fent);
    if (!fent) {
        syslog(LOG_ERR, "failed to find function (func='%s')", func_name);
        goto err;
    }

    return dialplan_call_func(fent, opt, ref);

err:
    return -1;
}

/**
 * @brief matches and calls a dial plan function.
 *
 * @param priority is priority to search dial plans.
 * @param num is phone number.
 * @param ref is reference data.
 * @param dialplan_call_rc is a return value that contains dialplan call result.
 *
 * @return 0 when it successfully finds and calls a dial plan function. Otherwise, -1.
 */
int dialplan_match_and_call_plan(int priority, const char* num, void* ref, int* dialplan_call_rc)
{
    struct dialplan_plan_entry_t* pent;
    struct dialplan_func_entry_t* fent;
    const char* opt;
    int rc = 0;

    int stat = -1;

    dialplan_for_each_plan_begin(pent) {

        if (!priority || pent->priority < priority) {
            opt = dialplan_match_plan(pent, num, &fent);
            if (opt && fent) {
                syslog(LOG_DEBUG, "dialplan matched, call func (num='%s',regex='%s',popt='%s',opt='%s',func='%s')", num, pent->regex,
                       pent->opt, opt, fent->func_name);

                rc = dialplan_call_func(fent, opt, ref);
                stat = 0;
                break;
            }
        }

        dialplan_for_each_plan_end();
    }

    /* return dialplan function return code */
    if (dialplan_call_rc) {
        *dialplan_call_rc = rc;
    }

    return stat;
}

#ifdef CONFIG_UNIT_TEST

int diaplan_module_test_call(const char* func_name, const char* opt, void* ref)
{
    printf("diaplan_module_test_call called (func=%s,opt=%s,ref=%p)", func_name, opt, ref);

    return 0;
}

int diaplan_module_test_ecall(const char* func_name, const char* opt, void* ref)
{
    printf("diaplan_module_test_ecall (func=%s,opt=%s,ref=%p)", func_name, opt, ref);

    return 0;
}


void diaplan_module_test()
{
    struct dialplan_plan_entry_t* pent;
    const char* opt;

    struct dialplan_func_entry_t* fent;

    dialplan_init();


    dialplan_add_func("call", diaplan_module_test_call, (void*)0);
    dialplan_add_func("ecall", diaplan_module_test_ecall, (void*)2);

    dialplan_parse_and_add_plan("10/^(04[0-9]{2})([0-9]{3})([0-9]{3})$/call/\\1\\2\\3");
    dialplan_parse_and_add_plan("1/^911$/ecall/\\0");
    dialplan_parse_and_add_plan("20/^([123456789][0-9]{3})([0-9]{4})$/call/prefix\\1\\2suffix");
    dialplan_parse_and_add_plan("15/^([123456789][0-9]{3})([0-9]{4})$/call/prefix\\1-\\2suffix");

    dialplan_sort_plan();

    dialplan_match_and_call_plan("0410305940");

    dialplan_for_each_plan_begin(pent) {

        opt = dialplan_match_plan(pent, "911", &fent);
        if (opt) {
            printf("0410305940 = %s", opt);
            dialplan_call_func(fent, opt);
        }

        opt = dialplan_match_plan(pent, "0410305940", &fent);
        if (opt) {
            printf("0410305940 = %s", opt);
            dialplan_call_func(fent, opt);
        }


        opt = dialplan_match_plan(pent, "74242017", &fent);
        if (opt) {
            printf("74242017 = %s", opt);
            dialplan_call_func(fent, opt);
        }

        dialplan_for_each_plan_end();
    }


    dialplan_fini();

}

int main(int argc, char* argv[])
{
    diaplan_module_test();
}

#endif




