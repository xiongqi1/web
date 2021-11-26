/*
 * NDD-0300 FaultMgmt Unit test
 *
 * Copyright Notice:
 * Copyright (C) 2017 NetComm Wireless limited.
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


#include <string.h>
#include <errno.h>

#include "mockrdblib.h"
#include "RdbHandler.h"

extern "C"
{
#include "fm_errno.h"
#include "fm_shared.h"
}

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <gtest_test_helpers.h>

using namespace ::testing;

namespace
{

class FaultMgmtSharedUtilsRdbTest : public RdbHandler, public ::testing::Test
{
public:
    FaultMgmtSharedUtilsRdbTest() {}

    void SetUp()
    {

    }

    void TearDown()
    {

    }
};

#define TEST_PATH             "TestPath.L1.L2"
#define TEST_STR_EMPTY        ""
#define TEST_STR_EMPTY_LEN    (sizeof(TEST_STR_EMPTY))
#define TEST_STR_0            "Test Value 0"
#define TEST_STR_0_LEN        (sizeof(TEST_STR_0))
#define TEST_STR_1            "Test Value 11"
#define TEST_STR_1_LEN        (sizeof(TEST_STR_1))

TEST_F(FaultMgmtSharedUtilsRdbTest, test_add_or_update_fail)
{
    ExpectRdbAddOrUpdate_FailGet(TEST_PATH);
    ASSERT_EQ(FAILED_GET_PARA, add_or_update(TEST_PATH, TEST_STR_0, TEST_STR_0_LEN, 0));

    ExpectRdbAddOrUpdate_FailCreate(TEST_PATH, TEST_STR_0);
    ASSERT_EQ(FAILED_CREATE_PARA, add_or_update(TEST_PATH, TEST_STR_0, TEST_STR_0_LEN, 0));

    ExpectRdbAddOrUpdate_FailSet(TEST_PATH, TEST_STR_0, TEST_STR_1);
    ASSERT_EQ(FAILED_SET_PARA, add_or_update(TEST_PATH, TEST_STR_1, TEST_STR_1_LEN, 0));
}

TEST_F(FaultMgmtSharedUtilsRdbTest, test_add_or_update_new)
{
    /* Empty string */
    ExpectRdbAddOrUpdate_Add(TEST_PATH, TEST_STR_EMPTY);
    ASSERT_EQ(0, add_or_update(TEST_PATH, TEST_STR_EMPTY, TEST_STR_EMPTY_LEN, 0));

    /* Non-empty string */
    ExpectRdbAddOrUpdate_Add(TEST_PATH, TEST_STR_0);
    ASSERT_EQ(0, add_or_update(TEST_PATH, TEST_STR_0, TEST_STR_0_LEN, 0));
}

TEST_F(FaultMgmtSharedUtilsRdbTest, test_add_or_update_existing)
{
    /* Equal strings */
    /* Empty */
    ExpectRdbAddOrUpdate_Skip(TEST_PATH, TEST_STR_EMPTY);
    ASSERT_EQ(0, add_or_update(TEST_PATH, TEST_STR_EMPTY, TEST_STR_EMPTY_LEN, 0));

    /* Non-empty */
    ExpectRdbAddOrUpdate_Skip(TEST_PATH, TEST_STR_0);
    ASSERT_EQ(0, add_or_update(TEST_PATH, TEST_STR_0, TEST_STR_0_LEN, 0));

    /* Inequal strings */
    /* Empty -> Non-empty*/
    ExpectRdbAddOrUpdate_Overwrite(TEST_PATH, TEST_STR_EMPTY, TEST_STR_0);
    ASSERT_EQ(0, add_or_update(TEST_PATH, TEST_STR_0, TEST_STR_0_LEN, 0));

    /* Non-empty -> Empty */
    ExpectRdbAddOrUpdate_Overwrite(TEST_PATH, TEST_STR_0, TEST_STR_EMPTY);
    ASSERT_EQ(0, add_or_update(TEST_PATH, TEST_STR_EMPTY, TEST_STR_EMPTY_LEN, 0));

    /* Non-empty -> Non-empty */
    ExpectRdbAddOrUpdate_Overwrite(TEST_PATH, TEST_STR_0, TEST_STR_1);
    ASSERT_EQ(0, add_or_update(TEST_PATH, TEST_STR_1, TEST_STR_1_LEN, 0));
}

TEST_F(FaultMgmtSharedUtilsRdbTest, test_get_number_fail)
{
    ExpectRdbGetInt(-ENOENT, TEST_PATH, 0);
    ASSERT_EQ(get_number(TEST_PATH), -1);

    ExpectRdbGetInt(-EBUSY, TEST_PATH, 0);
    ASSERT_EQ(get_number(TEST_PATH), -1);

    ExpectRdbGetInt(-EPERM, TEST_PATH, 0);
    ASSERT_EQ(get_number(TEST_PATH), -1);

    ExpectRdbGetInt(-EOVERFLOW, TEST_PATH, 0);
    ASSERT_EQ(get_number(TEST_PATH), -1);
}

TEST_F(FaultMgmtSharedUtilsRdbTest, test_get_number_success)
{
    ExpectRdbGetInt(0, TEST_PATH, -5);
    ASSERT_EQ(get_number(TEST_PATH), -5);

    ExpectRdbGetInt(0, TEST_PATH, -1);
    ASSERT_EQ(get_number(TEST_PATH), -1);

    ExpectRdbGetInt(0, TEST_PATH, 0);
    ASSERT_EQ(get_number(TEST_PATH), 0);

    ExpectRdbGetInt(0, TEST_PATH, 1);
    ASSERT_EQ(get_number(TEST_PATH), 1);

    ExpectRdbGetInt(0, TEST_PATH, 5);
    ASSERT_EQ(get_number(TEST_PATH), 5);
}

TEST_F(FaultMgmtSharedUtilsRdbTest, test_set_number_fail)
{
    ExpectRdbAddOrUpdate_FailGet(TEST_PATH);
    ASSERT_EQ(FAILED_SET_PARA, set_number(TEST_PATH, 0));

    ExpectRdbAddOrUpdate_FailCreate(TEST_PATH, "10");
    ASSERT_EQ(FAILED_SET_PARA, set_number(TEST_PATH, 10));

    ExpectRdbAddOrUpdate_FailSet(TEST_PATH, "10", "-15");
    ASSERT_EQ(FAILED_SET_PARA, set_number(TEST_PATH, -15));
}

TEST_F(FaultMgmtSharedUtilsRdbTest, test_set_number_success)
{
    ExpectRdbAddOrUpdate_Add(TEST_PATH, "-15");
    ASSERT_EQ(0, set_number(TEST_PATH, -15));

    ExpectRdbAddOrUpdate_Add(TEST_PATH, "-1");
    ASSERT_EQ(0, set_number(TEST_PATH, -1));

    ExpectRdbAddOrUpdate_Add(TEST_PATH, "0");
    ASSERT_EQ(0, set_number(TEST_PATH, 0));

    ExpectRdbAddOrUpdate_Add(TEST_PATH, "1");
    ASSERT_EQ(0, set_number(TEST_PATH, 1));

    ExpectRdbAddOrUpdate_Add(TEST_PATH, "16");
    ASSERT_EQ(0, set_number(TEST_PATH, 16));
}

TEST_F(FaultMgmtSharedUtilsRdbTest, test_count_object_id_in_index)
{
    ExpectRdbGetAlloc(-ENOENT, TEST_PATH, NULL);
    ASSERT_EQ(0, count_object_id_in_index(TEST_PATH));

    ExpectRdbGetAlloc(0, TEST_PATH, "");
    ASSERT_EQ(0, count_object_id_in_index(TEST_PATH));

    ExpectRdbGetAlloc(0, TEST_PATH, "1");
    ASSERT_EQ(1, count_object_id_in_index(TEST_PATH));

    ExpectRdbGetAlloc(0, TEST_PATH, "1,2,3,4");
    ASSERT_EQ(4, count_object_id_in_index(TEST_PATH));

    ExpectRdbGetAlloc(0, TEST_PATH, "1,2,3,4,-5");
    ASSERT_EQ(5, count_object_id_in_index(TEST_PATH));

    ExpectRdbGetAlloc(0, TEST_PATH, "1,2,3,4,-5,-6");
    ASSERT_EQ(6, count_object_id_in_index(TEST_PATH));
}

#define CREATE_STRING(x, str)   do { \
                                    x = (char *)malloc(strlen((str)) + 1); \
                                    strcpy(x, str); \
                                } while(0)

#define DELETE_STRING(x)        do { \
                                    free(x); x = NULL; \
                                } while(0)

#define TEST_STRING_ACTION(str, old_str, action, value, new_str)    \
                                do { \
                                    CREATE_STRING((str), (old_str)); \
                                    EXPECT_EQ((action)(&(str), (value)), strlen((new_str))); \
                                    ASSERT_STREQ((str), (new_str)); \
                                    DELETE_STRING((str)); \
                                } while (0)

class FaultMgmtSharedUtilsIndexTest : public ::testing::Test
{
public:
    FaultMgmtSharedUtilsIndexTest() {}

    void SetUp()
    {
        //::testing::FLAGS_gtest_throw_on_failure = true;
        //::testing::FLAGS_gtest_break_on_failure = true;
        ::testing::FLAGS_gtest_death_test_style = "threadsafe";

        input_string = NULL;
    }

    void TearDown()
    {

    }

    char *input_string;
};

TEST_F(FaultMgmtSharedUtilsIndexTest, test_count)
{
    ASSERT_EQ(0, count_index(""));
    ASSERT_EQ(0, count_index(","));
    ASSERT_EQ(0, count_index(",,"));

    ASSERT_EQ(1, count_index("1"));
    ASSERT_EQ(1, count_index(",1"));
    ASSERT_EQ(1, count_index("1,"));
    ASSERT_EQ(1, count_index(",1,"));

    ASSERT_EQ(2, count_index("1,11"));
    ASSERT_EQ(2, count_index(",1,11"));
    ASSERT_EQ(2, count_index("1,11,"));
    ASSERT_EQ(2, count_index(",1,11,"));
    ASSERT_EQ(2, count_index(",1,,11,"));

    ASSERT_EQ(3, count_index("1,11,311"));
    ASSERT_EQ(3, count_index(",1,11,311"));
    ASSERT_EQ(3, count_index("1,11,311,"));
    ASSERT_EQ(3, count_index(",1,11,311,"));

    ASSERT_EQ(5, count_index("1,2,3,4,5"));
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_find_input)
{
    int res = -100, start = -100, end = -100;

    /* Make sure no crash */
    res = find_index(NULL, 1, NULL, NULL);
    ASSERT_EQ(res, -1);

    res = find_index("1,2,3", 1, NULL, NULL);
    ASSERT_EQ(res, 0);

    res = find_index("1,2,3", 1, &start, NULL);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(start, 0);

    res = find_index("1,2,3", 1, NULL, &end);
    ASSERT_EQ(res, 0);
    ASSERT_EQ(end, 1);

    /* Make sure return matches start */
    res = find_index(NULL, 1, &start, &end);
    EXPECT_EQ(res, start);
    ASSERT_EQ(res, -1);

    res = find_index("1,2,3", 1, &start, &end);
    EXPECT_EQ(res, start);
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_find_at_begining)
{
    int start = -100, end = -100;
    int i;

    // One digit number
    EXPECT_EQ(0, find_index("1,2,3", 1, &start, &end));
    EXPECT_EQ(0, start);
    EXPECT_EQ(1, end);

    EXPECT_EQ(1, find_index(",1,2,3", 1, &start, &end));
    EXPECT_EQ(1, start);
    EXPECT_EQ(2, end);

    EXPECT_EQ(1, find_index(",1,2,3,", 1, &start, &end));
    EXPECT_EQ(1, start);
    EXPECT_EQ(2, end);

    // Multiple digits numbers
    EXPECT_EQ(0, find_index("12,13,14", 12, &start, &end));
    EXPECT_EQ(0, start);
    EXPECT_EQ(2, end);

    EXPECT_EQ(0, find_index("123,124,125", 123, &start, &end));
    EXPECT_EQ(0, start);
    EXPECT_EQ(3, end);
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_find_in_middle)
{
    int start = -100, end = -100;

    // One digit number
    EXPECT_EQ(2, find_index("1,2,3", 2, &start, &end));
    EXPECT_EQ(2, start);
    EXPECT_EQ(3, end);

    // Multiple digits numbers
    EXPECT_EQ(3, find_index("12,13,14", 13, &start, &end));
    EXPECT_EQ(3, start);
    EXPECT_EQ(5, end);

    EXPECT_EQ(4, find_index("123,124,125", 124, &start, &end));
    EXPECT_EQ(4, start);
    EXPECT_EQ(7, end);
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_find_at_end)
{
    int start = -100, end = -100;

    // One digit number
    EXPECT_EQ(4, find_index("1,2,3", 3, &start, &end));
    EXPECT_EQ(4, start);
    EXPECT_EQ(5, end);

    EXPECT_EQ(4, find_index("1,2,3,", 3, &start, &end));
    EXPECT_EQ(4, start);
    EXPECT_EQ(5, end);

    // Multiple digits numbers
    EXPECT_EQ(6, find_index("12,13,14", 14, &start, &end));
    EXPECT_EQ(6, start);
    EXPECT_EQ(8, end);

    EXPECT_EQ(8, find_index("123,124,125", 125, &start, &end));
    EXPECT_EQ(8, start);
    EXPECT_EQ(11, end);
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_find_none)
{
    int start = -100, end = -100;

    EXPECT_LT(find_index("", 3, &start, &end), 0);
    EXPECT_LT(start, 0);
    EXPECT_LT(end, 0);

    EXPECT_LT(find_index(",", 3, &start, &end), 0);
    EXPECT_LT(start, 0);
    EXPECT_LT(end, 0);

    EXPECT_LT(find_index(" ", 3, &start, &end), 0);
    EXPECT_LT(start, 0);
    EXPECT_LT(end, 0);

    EXPECT_LT(find_index("1,2,3", 4, &start, &end), 0);
    EXPECT_LT(start, 0);
    EXPECT_LT(end, 0);

    EXPECT_LT(find_index("11,12,13", 1, &start, &end), 0);
    EXPECT_LT(start, 0);
    EXPECT_LT(end, 0);
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_add_new)
{
    EXPECT_EQ(add_index(&input_string, 1), 1);
    ASSERT_STREQ(input_string, "1");
    DELETE_STRING(input_string);

    TEST_STRING_ACTION(input_string, "",            add_index,  1,  "1");
    TEST_STRING_ACTION(input_string, ",",           add_index,  1,  "1");
    TEST_STRING_ACTION(input_string, "1,2,3",       add_index,  4,  "1,2,3,4");
    TEST_STRING_ACTION(input_string, "1,2,3,",      add_index,  4,  "1,2,3,4");
    TEST_STRING_ACTION(input_string, ",1,2,3",      add_index,  4,  "1,2,3,4");
    TEST_STRING_ACTION(input_string, ",1,2,3,",     add_index,  4,  "1,2,3,4");
    TEST_STRING_ACTION(input_string, "11,12,13",    add_index,  1,  "11,12,13,1");
    TEST_STRING_ACTION(input_string, "111,122,133", add_index,  11, "111,122,133,11");
    TEST_STRING_ACTION(input_string, "100,101,102", add_index,  103, "100,101,102,103");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_add_existing_at_begining)
{
    TEST_STRING_ACTION(input_string, "1,2,3,4",      add_index,  1,  "1,2,3,4");
    TEST_STRING_ACTION(input_string, "11,12,13,14",  add_index,  11, "11,12,13,14");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_add_existing_in_middle)
{
    TEST_STRING_ACTION(input_string, "11,12,1,14",      add_index, 1, "11,12,1,14");
    TEST_STRING_ACTION(input_string, "11,12,13,14",     add_index, 13, "11,12,13,14");
    TEST_STRING_ACTION(input_string, ",11,12,13,14",    add_index, 13, "11,12,13,14");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_add_existing_at_end)
{
    TEST_STRING_ACTION(input_string, "11,12,13,1",  add_index, 1,  "11,12,13,1");
    TEST_STRING_ACTION(input_string, "11,12,13,14", add_index, 14,  "11,12,13,14");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_remove_none)
{
    EXPECT_EQ(remove_index(&input_string, 1), 0);
    ASSERT_STREQ(input_string, "");
    DELETE_STRING(input_string);

    TEST_STRING_ACTION(input_string, "",            remove_index, 1, "");
    TEST_STRING_ACTION(input_string, ",",           remove_index, 1, "");
    TEST_STRING_ACTION(input_string, "1,2,3",       remove_index, 4, "1,2,3");
    TEST_STRING_ACTION(input_string, "11,12,13",    remove_index, 1, "11,12,13");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_remove_last_one)
{
    TEST_STRING_ACTION(input_string, "1",       remove_index, 1,    "");
    TEST_STRING_ACTION(input_string, ",1",      remove_index, 1,    "");
    TEST_STRING_ACTION(input_string, "1,",      remove_index, 1,    "");
    TEST_STRING_ACTION(input_string, ",1,",     remove_index, 1,    "");
    TEST_STRING_ACTION(input_string, "11",      remove_index, 11,   "");
    TEST_STRING_ACTION(input_string, ",11",     remove_index, 11,   "");
    TEST_STRING_ACTION(input_string, "11,",     remove_index, 11,   "");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_remove_from_begining)
{
    TEST_STRING_ACTION(input_string, "1,2,3",       remove_index, 1,    "2,3");
    TEST_STRING_ACTION(input_string, ",1,2,3",      remove_index, 1,    "2,3");
    TEST_STRING_ACTION(input_string, "1,2,3,",      remove_index, 1,    "2,3");
    TEST_STRING_ACTION(input_string, ",1,2,3,",     remove_index, 1,    "2,3");
    TEST_STRING_ACTION(input_string, "11,12,13",    remove_index, 11,   "12,13");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_remove_from_middle)
{
    TEST_STRING_ACTION(input_string, "1,2,3",       remove_index, 2,    "1,3");
    TEST_STRING_ACTION(input_string, ",1,2,3",      remove_index, 2,    "1,3");
    TEST_STRING_ACTION(input_string, "1,2,3,",      remove_index, 2,    "1,3");
    TEST_STRING_ACTION(input_string, ",1,2,3,",     remove_index, 2,    "1,3");
    TEST_STRING_ACTION(input_string, "11,12,13",    remove_index, 12,   "11,13");
}

TEST_F(FaultMgmtSharedUtilsIndexTest, test_remove_from_end)
{
    TEST_STRING_ACTION(input_string, "1,2,3",       remove_index, 3,    "1,2");
    TEST_STRING_ACTION(input_string, "1,2,3,",      remove_index, 3,    "1,2");
    TEST_STRING_ACTION(input_string, ",1,2,3",      remove_index, 3,    "1,2");
    TEST_STRING_ACTION(input_string, ",1,2,3,",     remove_index, 3,    "1,2");
    TEST_STRING_ACTION(input_string, "11,12,13",    remove_index, 13,   "11,12");
}

} // namespace
