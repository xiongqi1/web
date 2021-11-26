#ifndef STORE_BOT_H_14073626052020
#define STORE_BOT_H_14073626052020
/*
 * Storage bot for metrics that need the full samples within a measurement period
 *
 * Copyright Notice:
 * Copyright (C) 2020 Casa Systems.
 *
 * This file or portions thereof may not be copied or distributed in any form
 * (including but not limited to printed or electronic forms and binary or object forms)
 * without the expressed written consent of Casa Systems.
 * Copyright laws and International Treaties protect the contents of this file.
 * Unauthorized use is prohibited.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY CASA SYSTEMS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CASA
 * SYSTEMS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* stats types the store bot can handle */
enum stat_type {
    STAT_TYPE_INT, /* 32-bit integer. We could extend to 64-bit if necessary */
    STAT_TYPE_FLOAT, /* single precision float (unused so far) */
    STAT_TYPE_STRING, /* string of any length (unused so far) */
};

/* struct to store all samples of a single item within a measurement period */
struct store_bot_t {
    int len; /* the max number of samples this bot can store */
    void * store; /* the pointer to the memory buffer to hold the samples */
    enum stat_type type; /* the type of samples */
    int sz; /* size of each sample */
    int pos; /* the index to the next empty spot */
    int (*cmp_func)(const void*, const void*); /* comparison function for the respective type */
};

int store_bot_init(struct store_bot_t* sb, int len, enum stat_type type);

void store_bot_fini(struct store_bot_t* sb);

int store_bot_reinit(struct store_bot_t* sb, int len, enum stat_type type);

void store_bot_reset(struct store_bot_t* sb);

int store_bot_feed(struct store_bot_t* sb, const void* sample);

void* store_bot_get_by_percentile(struct store_bot_t* sb, int percentile);

void* store_bot_get_by_majority(struct store_bot_t* sb);

void* store_bot_get_by_sum(struct store_bot_t* sb);

#endif
