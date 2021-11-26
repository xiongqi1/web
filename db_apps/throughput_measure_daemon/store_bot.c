#include "store_bot.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*
 * compare function for two integers
 *
 * @param a A pointer to the first integer
 * @param b A pointer to the second integer
 * @return 0 if equal; a negative integer if a < b; a postive integer if a > b
 */
static int cmp_int(const void* a, const void* b)
{
    return *(const int*)a - *(const int*)b;
}

/*
 * compare function for two floats
 *
 * @param a A pointer to the first float
 * @param b A pointer to the second float
 * @return 0 if equal; -1 if a < b; 1 if a > b
 */
static int cmp_float(const void* a, const void* b)
{
    float d = *(const float*)a - *(const float*)b;
    if (d > 0) {
        return 1;
    }
    if (d < 0) {
        return -1;
    }
    return 0;
}

/*
 * compare function for two strings
 *
 * @param a A pointer to the address of the first string
 * @param b A pointer to the address of the second string
 * @return 0 if equal; -1 if a < b; 1 if a > b
 */
static int cmp_string(const void* a, const void* b)
{
    const char* sa = *(const char * const *)a;
    const char* sb = *(const char * const *)b;
    return strcmp(sa, sb);
}

/*
 * initialise the bot
 *
 * @param sb A pointer to the store bot
 * @param len The max number of samples this bot can store
 * @param type The type of samples
 * @return 0 on success; -1 on failure
 */
int store_bot_init(struct store_bot_t* sb, int len, enum stat_type type)
{
    assert(sb);
    if (len < 1) {
        return -1;
    }
    switch(type) {
    case STAT_TYPE_INT:
        sb->sz = sizeof(int);
        sb->cmp_func = cmp_int;
        break;
    case STAT_TYPE_FLOAT:
        sb->sz = sizeof(float);
        sb->cmp_func = cmp_float;
        break;
    case STAT_TYPE_STRING:
        sb->sz = sizeof(char *);
        sb->cmp_func = cmp_string;
        break;
    default:
        assert(0);
    }
    sb->store = malloc(sb->sz * len);
    if (!sb->store) {
        return -1;
    }
    sb->type = type;
    sb->len = len;
    sb->pos = 0;

    return 0;
}

/*
 * release resource of the bot
 *
 * @param sb A pointer to the store bot
 */
void store_bot_fini(struct store_bot_t* sb)
{
    assert(sb);
    store_bot_reset(sb);
    free(sb->store);
    sb->store = NULL;
}

/*
 * reinitialise the bot with new parameters
 *
 * @see store_bot_init
 */
int store_bot_reinit(struct store_bot_t* sb, int len, enum stat_type type)
{
    store_bot_fini(sb);
    return store_bot_init(sb, len, type);
}

/*
 * reset the bot to prepare for next measurement period
 *
 * @param sb A pointer to the store bot
 */
void store_bot_reset(struct store_bot_t* sb)
{
    assert(sb);
    if (sb->type == STAT_TYPE_STRING) {
        /* for string type, we need to free the string buffers */
        for (sb->pos--; sb->pos >= 0; sb->pos--) {
            free(((char **)sb->store)[sb->pos]);
        }
    }
    sb->pos = 0;
}

/*
 * feed a sample to the bot
 *
 * @param sb A pointer to the store bot
 * @param sample A pointer to the sample. It must be able to cast to the
 * respective type of this bot
 * @return 0 on success; -1 on failure
 */
int store_bot_feed(struct store_bot_t* sb, const void* sample)
{
    assert(sb->store);
    if (sb->pos >= sb->len) {
        return -1;
    }
    switch(sb->type) {
    case STAT_TYPE_INT:
        ((int *)sb->store)[sb->pos++] = *(const int *)sample;
        break;
    case STAT_TYPE_FLOAT:
        ((float *)sb->store)[sb->pos++] = *(const float *)sample;
        break;
    case STAT_TYPE_STRING:
    {
        /* string is duplicated here, so need to free on completion */
        char * p = strdup((const char *)sample);
        if (!p) {
            return -1;
        }
        ((char **)sb->store)[sb->pos++] = p;
        break;
    }
    default:
        assert(0);
    }
    return 0;
}

/*
 * get the element stored at a given index
 *
 * @param sb A pointer to the store bot
 * @param ix The index to the intended element to be retrieved
 * @return A pointer to the element of interest
 */
inline static void* store_bot_at(struct store_bot_t* sb, int ix) {
    return (void *)((char *)sb->store + ix * sb->sz);
}

/*
 * get a measurement result by percentile method from all samples stored so far
 *
 * @param sb A pointer to the store bot
 * @param percentile The percentile value to be used to calculate the result
 * @return A pointer to the result on success; NULL on failure
 */
void* store_bot_get_by_percentile(struct store_bot_t* sb, int percentile)
{
    void* res;
    int ix;
    assert(sb);
    assert(percentile >=0 && percentile <= 100);
    if (sb->pos < 1) {
        return NULL;
    }
    /* sort sb->store[0..pos) first */
    qsort(sb->store, sb->pos, sb->sz, sb->cmp_func);
    ix = (int) ((long)sb->pos * (long)percentile / 100L);
    if (ix >= sb->pos) {
        ix = sb->pos - 1;
    }
    res = store_bot_at(sb, ix);
    if (sb->type == STAT_TYPE_STRING) {
        /* for string type, we return the pointer to the string instead of a pointer to pointer */
        res = *(char **)res;
    }
    return res;
}

/*
 * get a measurement result by majority method from all samples stored so far
 *
 * @param sb A pointer to the store bot
 * @return A pointer to the result on success; NULL on failure
 */
void* store_bot_get_by_majority(struct store_bot_t* sb)
{
    int i;
    int max_cnt, curr_cnt;
    void *res, *prev, *curr;
    assert(sb);
    if (sb->pos < 1) {
        return NULL;
    }
    /* sort sb->store[0..pos) first */
    qsort(sb->store, sb->pos, sb->sz, sb->cmp_func);
    max_cnt = curr_cnt = 1;
    res = prev = store_bot_at(sb, 0);
    for (i = 1; i < sb->pos; i++) {
        curr = store_bot_at(sb, i);
        if (!sb->cmp_func(prev, curr)) {
            curr_cnt++;
        } else {
            if (curr_cnt > max_cnt) {
                max_cnt = curr_cnt;
                res = prev;
            }
            prev = curr;
            curr_cnt = 1;
        }
    }
    if (curr_cnt > max_cnt) {
        res = prev;
    }
    if (sb->type == STAT_TYPE_STRING) {
        res = *(char **)res;
    }
    return res;
}

/*
 * get a measurement result by sum over all samples stored so far
 *
 * @param sb A pointer to the store bot
 * @return A pointer to the result on success; NULL on failure
 */
void* store_bot_get_by_sum(struct store_bot_t* sb)
{
    static int64_t sum_i;
    static float sum_f;
    int i;
    assert(sb);
    sum_i = 0;
    sum_f = 0.0f;
    for (i = 0; i < sb->pos; i++) {
        if (sb->type == STAT_TYPE_INT) {
            sum_i += *(int *)store_bot_at(sb, i);
        } else if (sb->type == STAT_TYPE_FLOAT) {
            sum_f += *(float *)store_bot_at(sb, i);
        } else {
            assert(0);
        }
    }
    return sb->type == STAT_TYPE_INT ? (void *)&sum_i : (void *)&sum_f;
}
