/**
 * C header file of moving average and distribute calculation
 *
 * Copyright Notice:
 * Copyright (C) 2015 NetComm Wireless limited.
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
 */

#ifndef __AVG_H__
#define __AVG_H__

struct distr_t {
    const char *name;

    int range;
    unsigned long long *c;
};

struct acc_t {
    const char *name;

    int separator;

    int valid;
    int time_valid;
    unsigned long long acc;  /* accumulated value */
    unsigned long long time; /* value update time */

    unsigned long long stime; /* value start time */
    unsigned long long sacc;  /* start accumulated value */

    int stat_valid;
    long long min;
    long long max;
    long long avg;
    int min_max_valid;

    unsigned long long diff; /* accumulate diff */
};

struct avg_t {
    const char *name;
    unsigned long long c_limit;

    int separator;

    int stat_valid;
    long double avg;      /* average value */
    unsigned long long c; /* average count */

    long long l; /* last value */

    int min;
    int max;
};

void acc_init(struct acc_t *a, const char *name);
void acc_reset(struct acc_t *a);
void acc_feed(struct acc_t *a, unsigned long long acc, const unsigned long long *time);
void acc_feed_time(struct acc_t *a, const unsigned long long *time);
unsigned long long acc_get_diff(struct acc_t *a);

void avg_init(struct avg_t *a, const char *name, unsigned long long limit);
void avg_feed(struct avg_t *a, long long new_val);
void avg_reset(struct avg_t *a);

void distr_fini(struct distr_t *d);
int distr_init(struct distr_t *d, const char *name, int range);
void distr_reset(struct distr_t *d);
int distr_feed(struct distr_t *d, int v);
const char *distr_get(struct distr_t *d);

#endif
