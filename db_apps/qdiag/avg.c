/*!
 * Implementation of moving average and distribute calculation
 *
 * Copyright Notice:
 * Copyright (C) 2014 NetComm Wireless limited.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of NetComm Wireless Ltd.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY NETCOMM WIRELESS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETCOMM
 * WIRELESS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avg.h"
#include "def.h"

#define __min_if_valid(min, new_val, min_valid) (((min_valid) && ((min) < (new_val))) ? (min) : (new_val))
#define __max_if_valid(max, new_val, max_valid) (((max_valid) && ((max) > (new_val))) ? (max) : (new_val))
#define __offset(struc, member) ((int)(&(((struc *)0)->member)))

/*
        ### distribute calculation ###
*/

/*
 Destroy an object of distribute calculation.

 Params:
  d : pointer to an object of distribute.

 Return:
  None.
*/
void distr_fini(struct distr_t *d)
{
    free(d->c);
}

/*
 Create an object of distribute calculation.

 Params:
  d : pointer to an object of distribute.
  name : object name - information only.
  rnage : distribute range.

 Return:
  0 = success. Otherwise, failure.
*/
int distr_init(struct distr_t *d, const char *name, int range)
{
    d->name = name;
    d->range = range;

    d->c = calloc(sizeof(*d->c), range);
    if (!d->c) {
        ERR("failed to allocate range (n=%s,range=%d)", d->name, d->range);
        goto err;
    }

    return 0;

err:
    return -1;
}

/*
 Reset information in an object of distribute calculation.

 Params:
  d : pointer to an object of distribute.

 Return:
  None.
*/
void distr_reset(struct distr_t *d)
{
    memset(d->c, 0, sizeof(*d->c) * d->range);
}

/*
 Get distribute calculation result.

 Params:
  d : pointer to an object of distribute.

 Return:
  Distribute calculation result in the format of "a,b,c,d".
*/
const char *distr_get(struct distr_t *d)
{
    static char line[1024];

    int avail = sizeof(line);
    int used;
    int i;

    char *p = line;

    *line = 0;

    for (i = 0; i < d->range; i++) {

        if (*line)
            used = snprintf(p, avail, ",%llu", d->c[i]);
        else
            used = snprintf(p, avail, "%llu", d->c[i]);

        p += used;
        avail -= used;
    }

    return line;
}

/*
 Feed an entry into a distribute calculation object

 Params:
  d : pointer to an object of distribute

 Return:
  0 = success. Otherwise, failure.
*/
int distr_feed(struct distr_t *d, int v)
{
    if (!d->c) {
        ERR("no range buffer allocated");
        goto err;
    }

    if (!(v < d->range)) {
        ERR("out of range (n=%s,v=%d,range=%d)", d->name, v, d->range);
        goto err;
    }

    d->c[v]++;
    if (!d->c[v]) {
        ERR("overflow occured, reset (n=%s)", d->name);
        distr_reset(d);
    }

    return 0;

err:
    return -1;
}

/*
        ### accumulate functions ###
*/

/*
 Get difference since the starting of accumulation

 Params:
  a : pointer to an object of accumulate

 Return:
  Difference since the starting of accumulation
*/
unsigned long long acc_get_diff(struct acc_t *a)
{
    return a->diff;
}
/*
 Calculate moving average for period based on time

 Params:
  a : pointer to an object of accumulate
  acc : accumulated value to feed to the object
  time : pointer to a Qualcomm time

 Return:
  None
*/

void acc_feed_time(struct acc_t *a, const unsigned long long *time)
{
    unsigned long long ms;

    ms = get_ms_from_ts(time);

    /* set initial time */
    if (!a->time_valid) {
        a->time = ms;
        a->stime = ms;
        a->time_valid = 1;
    }
}

void acc_feed(struct acc_t *a, unsigned long long acc, const unsigned long long *time)
{
    unsigned long long time_diff;
    unsigned long long time_diff_from_s;
    unsigned long long acc_diff;
    long double v;
    unsigned long long ms;

    ms = get_ms_from_ts(time);

    /*
        workaround for QC MPSS.TH.2.0.1-00282-M9645LAAAANAZM-1 - Some of accumulated fields occasionally goes backward.

        * issue
         This behavior could not be an issue. But it may happen due to asynchronization of QC diag messages.
        The last statistics of lte_pdcpdl_statistics appears after a new RRC established indication of event_lte_rrc_state_change.
        In result, when the first statistics appears in the new RRC session, accumulation goes backward.

        * solution
        This workaround invalidates any previous feeding when acc becomes 0. This is based on the assumption that acc does not overflow.
    */

    if (a->valid && a->sacc && !a->acc) {
        a->stat_valid = 0;
        a->min_max_valid = 0;
        a->valid = 0;
        a->time_valid = 0;
        DEBUG("[%s] zero accumulated value detected after valid feeding (sacc=%lld,acc=%lld)", a->name, a->sacc, acc);
    }

    /* set initial time */
    if (!a->time_valid) {
        a->time = ms;
        a->stime = ms;
        a->time_valid = 1;
    }

    if (!a->valid) {
        a->sacc = acc;
        a->valid = 1;
    }

    /* get difference */
    time_diff = ms - a->time;
    /* accumulate diff */
    acc_diff = (acc >= a->acc) ? (acc - a->acc) : 0;
    a->diff += acc_diff;
    time_diff_from_s = ms - a->stime;

    /* update members */
    a->time = ms;
    a->acc = acc;

    /* calculate instance delta */
    if (time_diff) {
        /* calculate v in measurement period */
        v = acc_diff * 1000 / time_diff;

        /* get min */
        a->min = __min_if_valid(a->min, v, a->min_max_valid);
        /* get max */
        a->max = __max_if_valid(a->max, v, a->min_max_valid);

        VERBOSE("[%s] time_diff=%llu", a->name, time_diff);
        VERBOSE("[%s] v=%.2Lf", a->name, v);
        VERBOSE("[%s] min=%lld", a->name, a->min);
        VERBOSE("[%s] max=%lld", a->name, a->max);

        a->min_max_valid = 1;
    }

    /* calculate average of total period */

    if (time_diff_from_s) {
        /* calculate total average */
        a->avg = a->diff * 1000 / time_diff_from_s;

        /* update min and max because of less samples for min. and max */
        /* get min */
        a->min = __min_if_valid(a->min, a->avg, a->min_max_valid);
        /* get max */
        a->max = __max_if_valid(a->max, a->avg, a->min_max_valid);

        VERBOSE("[%s] acc=%lld", a->name, acc);

        VERBOSE("[%s] acc_diff=%lld", a->name, acc_diff);

        VERBOSE("[%s] time_diff_from_s=%llu", a->name, time_diff_from_s);
        VERBOSE("[%s] sacc=%llu", a->name, a->sacc);
        VERBOSE("[%s] avg=%lld", a->name, a->avg);

        a->stat_valid = 1;
        a->min_max_valid = 1;
    }
}

/*
 Reset information in an object of accumulate calculation.

 Params:
  a : pointer to an object of accumulate

 Return:
  None
*/
void acc_reset(struct acc_t *a)
{
    int off = __offset(struct acc_t, separator);
    memset((char *)a + off, 0, sizeof(*a) - off);
}

/*
 Initiate an object of accumulate calculation.

 Params:
  a : pointer to an object of accumulate
  name : name of accumulate calculation - only debug information

 Return:
  None
*/
void acc_init(struct acc_t *a, const char *name)
{
    a->name = name;
    acc_reset(a);
}

/*
        ### average functions ###
*/

/*
 Feed an element for calculate moving average each time until limit.

 Params:
  a : pointer to an object of average calculation
  new_val : an element of average calculation

 Return:
  None
*/
void avg_feed(struct avg_t *a, long long new_val)
{
    /* increase count until limit if limit is setup but it is not allowed to overwrap */
    if ((a->c < ~0ULL) && (!a->c_limit || (a->c < a->c_limit)))
        a->c++;

    if (!a->c) {
        ERR("zero count found in %s", a->name);
        goto err;
    }

    a->l = new_val;

    /* get min */
    a->min = __min_if_valid(a->min, new_val, a->stat_valid);

    /* get max */
    a->max = __max_if_valid(a->max, new_val, a->stat_valid);

    /* get moving average */
    if (a->c < 2)
        a->avg = new_val;
    else
        a->avg = a->avg + ((long double)new_val - a->avg) / a->c;

    VERBOSE("[%s] c=%llu", a->name, a->c);
    VERBOSE("[%s] c_limit=%llu", a->name, a->c_limit);
    VERBOSE("[%s] min=%d", a->name, a->min);
    VERBOSE("[%s] max=%d", a->name, a->max);
    VERBOSE("[%s] avg=%.2Lf", a->name, a->avg);

    /* set valid */
    a->stat_valid = 1;

err:
    return;
}

/*
 Reset information in an object of average calculation.

 Params:
  a : pointer to an object of average.

 Return:
  None.
*/
void avg_reset(struct avg_t *a)
{
    int off = __offset(struct avg_t, separator);

    memset((char *)a + off, 0, sizeof(*a) - off);
}

/*
 Initiate an object of average calculation.

 Params:
  a : pointer to an object of average.
  name : name of an object of average - debug information
  c_limit : moving average weight  limit

 Return:
  None.
*/
void avg_init(struct avg_t *a, const char *name, unsigned long long c_limit)
{
    a->name = name;
    a->c_limit = c_limit;

    avg_reset(a);
}
