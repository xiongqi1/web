/*
 * store_bot unit test
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
#include "../store_bot.h"
#include <unit_test.h>

void test_percentile(enum stat_type type)
{
    struct store_bot_t sb;
    const int len = 100;
    int i;
    void* v;

    TEST_CHECK(!store_bot_init(&sb, len, type));
    TEST_CHECK(sb.len == len);
    TEST_CHECK(sb.type == type);
    if (type == STAT_TYPE_INT) {
        TEST_CHECK(sb.sz == sizeof(int));
    } else if (type == STAT_TYPE_FLOAT) {
        TEST_CHECK(sb.sz == sizeof(float));
    } else {
        TEST_CHECK(sb.sz == sizeof(char *));
    }
    TEST_CHECK(sb.pos == 0);

    int sample = len - 1;
    float sample_f;
    char buf[3];
    for (i = 0; i < len; i++) {
        if (type == STAT_TYPE_INT) {
            TEST_CHECK(!store_bot_feed(&sb, &sample));
            TEST_CHECK(sb.pos == i + 1);
            TEST_CHECK(((int *)sb.store)[i] == sample);
        } else if (type == STAT_TYPE_FLOAT) {
            sample_f = sample;
            TEST_CHECK(!store_bot_feed(&sb, &sample_f));
            TEST_CHECK(sb.pos == i + 1);
            TEST_CHECK(((float *)sb.store)[i] == sample_f);
        } else {
            sprintf(buf, "%02d", sample);
            TEST_CHECK(!store_bot_feed(&sb, buf));
            TEST_CHECK(sb.pos == i + 1);
            TEST_CHECK(!strcmp(((char **)sb.store)[i], buf));
        }
        sample--;
    }
    sample = len - 1;
    for (i = 0; i < len; i++) {
        if (type == STAT_TYPE_INT) {
            TEST_CHECK(((int *)sb.store)[i] == sample);
        } else if (type == STAT_TYPE_FLOAT) {
            sample_f = sample;
            TEST_CHECK(((float *)sb.store)[i] == sample_f);
        } else {
            sprintf(buf, "%02d", sample);
            TEST_CHECK(!strcmp(((char **)sb.store)[i], buf));
        }
        sample--;
    }
    for (i = 0; i < len; i++) {
        v = store_bot_get_by_percentile(&sb, i);
        if (type == STAT_TYPE_INT) {
            TEST_CHECK(*(int *)v == i);
        } else if (type == STAT_TYPE_FLOAT) {
            sample_f = i;
            TEST_CHECK(*(float *)v == sample_f);
        } else {
            sprintf(buf, "%02d", i);
            TEST_CHECK(!strcmp((char *)v, buf));
        }
    }

    v = store_bot_get_by_majority(&sb);
    if (type == STAT_TYPE_INT) {
        TEST_CHECK(*(int *)v == 0);
    } else if (type == STAT_TYPE_FLOAT) {
        TEST_CHECK(*(float *)v == 0.0f);
    } else {
        TEST_CHECK(!strcmp((char *)v, "00"));
    }

    if (type == STAT_TYPE_INT) {
        v = store_bot_get_by_sum(&sb);
        TEST_CHECK(*(int *)v ==  len * (len - 1) / 2);
    }

    store_bot_fini(&sb);
}

void test_majority(enum stat_type type)
{
    struct store_bot_t sb;
    const int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 1, 2, 5, 7, 11, 3, 4, 2, 15, 1, 2, 3};
    const int len = sizeof(data) / sizeof(data[0]);
    int i;
    int val_i;
    float val_f;
    char buf[3];
    void* v;
    TEST_CHECK(!store_bot_init(&sb, len, type));
    TEST_CHECK(sb.len == len);
    TEST_CHECK(sb.type == type);
    if (type == STAT_TYPE_INT) {
        TEST_CHECK(sb.sz == sizeof(int));
    } else if (type == STAT_TYPE_FLOAT) {
        TEST_CHECK(sb.sz == sizeof(float));
    } else {
        TEST_CHECK(sb.sz == sizeof(char *));
    }
    TEST_CHECK(sb.pos == 0);
    for (i = 0; i < len; i++) {
        if (type == STAT_TYPE_INT) {
            val_i = data[i];
            TEST_CHECK(!store_bot_feed(&sb, &val_i));
            TEST_CHECK(((int *)sb.store)[i] == val_i);
        } else if (type == STAT_TYPE_FLOAT) {
            val_f = data[i];
            TEST_CHECK(!store_bot_feed(&sb, &val_f));
            TEST_CHECK(((float *)sb.store)[i] == val_f);
        } else {
            sprintf(buf, "%02d", data[i]);
            TEST_CHECK(!store_bot_feed(&sb, buf));
            TEST_CHECK(!strcmp(((char **)sb.store)[i], buf));
        }
        TEST_CHECK(sb.pos == i + 1);
    }
    v = store_bot_get_by_majority(&sb);
    if (type == STAT_TYPE_INT) {
        TEST_CHECK(*(int *)v == 2);
    } else if (type == STAT_TYPE_FLOAT) {
        TEST_CHECK(*(float *)v == 2.0f);
    } else {
        TEST_CHECK(!strcmp((char *)v,"02"));
    }
    store_bot_fini(&sb);
}

void test_majority_inorder(enum stat_type type)
{
    struct store_bot_t sb;
    const int data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 10, 10};
    const int len = sizeof(data) / sizeof(data[0]);
    int i;
    int val_i;
    float val_f;
    char buf[3];
    void* v;
    TEST_CHECK(!store_bot_init(&sb, len, type));
    TEST_CHECK(sb.len == len);
    TEST_CHECK(sb.type == type);
    if (type == STAT_TYPE_INT) {
        TEST_CHECK(sb.sz == sizeof(int));
    } else if (type == STAT_TYPE_FLOAT) {
        TEST_CHECK(sb.sz == sizeof(float));
    } else {
        TEST_CHECK(sb.sz == sizeof(char *));
    }
    TEST_CHECK(sb.pos == 0);
    for (i = 0; i < len; i++) {
        if (type == STAT_TYPE_INT) {
            val_i = data[i];
            TEST_CHECK(!store_bot_feed(&sb, &val_i));
            TEST_CHECK(((int *)sb.store)[i] == val_i);
        } else if (type == STAT_TYPE_FLOAT) {
            val_f = data[i];
            TEST_CHECK(!store_bot_feed(&sb, &val_f));
            TEST_CHECK(((float *)sb.store)[i] == val_f);
        } else {
            sprintf(buf, "%02d", data[i]);
            TEST_CHECK(!store_bot_feed(&sb, buf));
            TEST_CHECK(!strcmp(((char **)sb.store)[i], buf));
        }
        TEST_CHECK(sb.pos == i + 1);
    }
    v = store_bot_get_by_majority(&sb);
    if (type == STAT_TYPE_INT) {
        TEST_CHECK(*(int *)v == 10);
    } else if (type == STAT_TYPE_FLOAT) {
        TEST_CHECK(*(float *)v == 10.0f);
    } else {
        TEST_CHECK(!strcmp((char *)v,"10"));
    }
    store_bot_fini(&sb);
}

void test_reset_int()
{
    struct store_bot_t sb;
    const int len = 10;
    const int data0[] = {1, 3, 8, 10, 50};
    const int data1[] = {-100, -300, 250, 485, 600, -92};
    const int data2[] = {-1, -2, -3, -4, -5, -6, 1, 3, 5, 7};
    const int data3[] = {10, -10, -15, -20, 0, -3, 4, 5, 8, -2};

    int i;
    TEST_CHECK(!store_bot_init(&sb, len, STAT_TYPE_INT));
    TEST_CHECK(sb.len == len);
    TEST_CHECK(sb.type == STAT_TYPE_INT);
    TEST_CHECK(sb.sz == sizeof(int));
    TEST_CHECK(sb.pos == 0);

    for (i = 0; i < sizeof(data0)/sizeof(data0[0]); i++) {
        TEST_CHECK(!store_bot_feed(&sb, &data0[i]));
    }
    TEST_CHECK(sb.pos == i);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 0) == data0[0]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 19) == data0[0]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 20) == data0[1]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 39) == data0[1]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 40) == data0[2]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 59) == data0[2]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 60) == data0[3]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 79) == data0[3]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 80) == data0[4]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 99) == data0[4]);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 100) == data0[4]);

    store_bot_reset(&sb);
    for (i = 0; i < sizeof(data1)/sizeof(data1[0]); i++) {
        TEST_CHECK(!store_bot_feed(&sb, &data1[i]));
    }
    TEST_CHECK(sb.pos == i);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 0) == -300);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 17) == -100);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 34) == -92);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 50) == 250);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 67) == 485);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 84) == 600);

    store_bot_reset(&sb);
    for (i = 0; i < sizeof(data2)/sizeof(data2[0]); i++) {
        TEST_CHECK(!store_bot_feed(&sb, &data2[i]));
    }
    TEST_CHECK(sb.pos == i);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 0) == -6);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 10) == -5);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 20) == -4);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 30) == -3);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 40) == -2);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 50) == -1);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 60) == 1);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 70) == 3);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 80) == 5);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 90) == 7);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 100) == 7);

    store_bot_reset(&sb);
    for (i = 0; i < sizeof(data3)/sizeof(data3[0]); i++) {
        TEST_CHECK(!store_bot_feed(&sb, &data3[i]));
    }
    TEST_CHECK(sb.pos == i);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 0) == -20);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 10) == -15);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 20) == -10);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 30) == -3);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 40) == -2);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 50) == 0);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 60) == 4);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 70) == 5);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 80) == 8);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 90) == 10);
    TEST_CHECK(*(int *)store_bot_get_by_percentile(&sb, 100) == 10);

    store_bot_fini(&sb);
}

void test_reset_string()
{
    struct store_bot_t sb;
    const char* data[] = {"aaaaaa", "bbbbbb", "cccccc", "dddddd", "eeeeee", "ffffff", "gggggg", "hhhhhh", "iiiiii", "gggggg"};
    const int len = sizeof(data) / sizeof(data[0]);
    int i;
    int repeats = 100000;
    TEST_CHECK(!store_bot_init(&sb, len, STAT_TYPE_STRING));
    for (; repeats > 0; repeats--) {
        for (i = 0; i < len; i++) {
            TEST_CHECK(!store_bot_feed(&sb, data[i]));
        }
        for (i = 0; i < len; i++) {
            TEST_CHECK(!strcmp(((char **)sb.store)[i], data[i]));
        }
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 0), "aaaaaa"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 10), "bbbbbb"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 20), "cccccc"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 30), "dddddd"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 40), "eeeeee"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 50), "ffffff"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 60), "gggggg"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 70), "gggggg"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 80), "hhhhhh"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 90), "iiiiii"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_percentile(&sb, 100), "iiiiii"));
        TEST_CHECK(!strcmp((char *)store_bot_get_by_majority(&sb), "gggggg"));

        store_bot_reset(&sb);
    }
    store_bot_fini(&sb);
}

void test_invalid_int()
{
    struct store_bot_t sb;
    int val = 0;
    TEST_CHECK(store_bot_init(&sb, 0, STAT_TYPE_INT) < 0);
    TEST_CHECK(!store_bot_init(&sb, 1, STAT_TYPE_INT));
    TEST_CHECK(!store_bot_feed(&sb, &val));
    TEST_CHECK(store_bot_feed(&sb, &val) < 0);
    store_bot_fini(&sb);
}

void TEST_AUTO(test_percentile_int)
{
    test_percentile(STAT_TYPE_INT);
}
void TEST_AUTO(test_percentile_float)
{
    test_percentile(STAT_TYPE_FLOAT);
}
void TEST_AUTO(test_percentile_string)
{
    test_percentile(STAT_TYPE_STRING);
}

void TEST_AUTO(test_majority_int)
{
    test_majority(STAT_TYPE_INT);
}

void TEST_AUTO(test_majority_float)
{
    test_majority(STAT_TYPE_FLOAT);
}

void TEST_AUTO(test_majority_string)
{
    test_majority(STAT_TYPE_STRING);
}

void TEST_AUTO(test_majority_inorder_int)
{
    test_majority_inorder(STAT_TYPE_INT);
}

void TEST_AUTO(test_majority_inorder_float)
{
    test_majority_inorder(STAT_TYPE_FLOAT);
}

void TEST_AUTO(test_majority_inorder_string)
{
    test_majority_inorder(STAT_TYPE_STRING);
}

void TEST_AUTO(test_reset_int)
{
    test_reset_int();
}

void TEST_AUTO(test_reset_string)
{
    test_reset_string();
}

void TEST_AUTO(test_invalid_int)
{
    test_invalid_int();
}

TEST_AUTO_MAIN
